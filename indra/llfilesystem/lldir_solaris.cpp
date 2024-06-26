/** 
 * @file fmodwrapper.cpp
 * @brief dummy source file for building a shared library to wrap libfmod.a
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "lldir_solaris.h"
#include "llerror.h"
#include "llrand.h"
#include "llstring.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>
#include <pwd.h>
#include <sys/utsname.h>
#define _STRUCTURED_PROC 1
#include <sys/procfs.h>
#include <fcntl.h>

static std::string getCurrentUserHome(char* fallback)
{
	// fwiw this exactly duplicates getCurrentUserHome() in lldir_linux.cpp...
	// we should either derive both from LLDir_Posix or just axe Solaris.
	const uid_t uid = getuid();
	struct passwd *pw;

	pw = getpwuid(uid);
	if ((pw != NULL) && (pw->pw_dir != NULL))
	{
		return pw->pw_dir;
	}

	LL_INFOS() << "Couldn't detect home directory from passwd - trying $HOME" << LL_ENDL;
	auto home_env = LLStringUtil::getoptenv("HOME");
	if (home_env)
	{
		return *home_env;
	}
	else
	{
		LL_WARNS() << "Couldn't detect home directory!  Falling back to " << fallback << LL_ENDL;
		return fallback;
	}
}


LLDir_Solaris::LLDir_Solaris()
{
	mDirDelimiter = "/";
	mCurrentDirIndex = -1;
	mCurrentDirCount = -1;
	mDirp = NULL;

	char tmp_str[LL_MAX_PATH];	/* Flawfinder: ignore */ 
	if (getcwd(tmp_str, LL_MAX_PATH) == NULL)
	{
		strcpy(tmp_str, "/tmp");
		LL_WARNS() << "Could not get current directory; changing to "
				<< tmp_str << LL_ENDL;
		if (chdir(tmp_str) == -1)
		{
			LL_ERRS() << "Could not change directory to " << tmp_str << LL_ENDL;
		}
	}

	mExecutableFilename = "";
	mExecutablePathAndName = "";
	mExecutableDir = strdup(tmp_str);
	mWorkingDir = strdup(tmp_str);
	mAppRODataDir = strdup(tmp_str);
	mOSUserDir = getCurrentUserHome(tmp_str);
	mOSUserAppDir = "";
	mLindenUserDir = "";

	char path [LL_MAX_PATH];	/* Flawfinder: ignore */ 

	sprintf(path, "/proc/%d/psinfo", (int)getpid());
	int proc_fd = -1;
	if((proc_fd = open(path, O_RDONLY)) == -1){
		LL_WARNS() << "unable to open " << path << LL_ENDL;
		return;
	}
	psinfo_t proc_psinfo;
	if(read(proc_fd, &proc_psinfo, sizeof(psinfo_t)) != sizeof(psinfo_t)){
		LL_WARNS() << "Unable to read " << path << LL_ENDL;
		close(proc_fd);
		return;
	}

	close(proc_fd);

	mExecutableFilename = strdup(proc_psinfo.pr_fname);
	LL_INFOS() << "mExecutableFilename = [" << mExecutableFilename << "]" << LL_ENDL;

	sprintf(path, "/proc/%d/path/a.out", (int)getpid());

	char execpath[LL_MAX_PATH];
	if(readlink(path, execpath, LL_MAX_PATH) == -1){
		LL_WARNS() << "Unable to read link from " << path << LL_ENDL;
		return;
	}

	char *p = execpath;			// nuke trash in link, if any exists
	int i = 0;
	while(*p != NULL && ++i < LL_MAX_PATH && isprint((int)(*p++)));
	*p = NULL;

	mExecutablePathAndName = strdup(execpath);
	LL_INFOS() << "mExecutablePathAndName = [" << mExecutablePathAndName << "]" << LL_ENDL;

	//NOTE: Why force people to cd into the package directory?
	//      Look for SECONDLIFE env variable and use it, if set.

	auto SECONDLIFE(LLDirUtil::getoptenv("BLACKDRAGON"));
	if(BLACKDRAGON){
		mExecutableDir = add(*BLACKDRAGON, "bin"); //NOTE:  make sure we point at the bin
	}else{
		mExecutableDir = getDirName(execpath);
		LL_INFOS() << "mExecutableDir = [" << mExecutableDir << "]" << LL_ENDL;
	}

	mLLPluginDir = add(mExecutableDir, "llplugin");

	// *TODO: don't use /tmp, use $HOME/.secondlife/tmp or something.
	mTempDir = "/tmp";
}

LLDir_Solaris::~LLDir_Solaris()
{
}

// Implementation


void LLDir_Solaris::initAppDirs(const std::string &app_name,
								const std::string& app_read_only_data_dir)
{
	// Allow override so test apps can read newview directory
	if (!app_read_only_data_dir.empty())
	{
		mAppRODataDir = app_read_only_data_dir;
		mSkinBaseDir = add(mAppRODataDir, "skins");
	}
	mAppName = app_name;

	std::string upper_app_name(app_name);
	LLStringUtil::toUpper(upper_app_name);

	auto app_home_env(LLStringUtil::getoptenv(upper_app_name + "_USER_DIR"));
	if (app_home_env)
	{
		// user has specified own userappdir i.e. $SECONDLIFE_USER_DIR
		mOSUserAppDir = *app_home_env;
	}
	else
	{
		// traditionally on unixoids, MyApp gets ~/.myapp dir for data
		mOSUserAppDir = mOSUserDir;
		mOSUserAppDir += "/";
		mOSUserAppDir += ".";
		std::string lower_app_name(app_name);
		LLStringUtil::toLower(lower_app_name);
		mOSUserAppDir += lower_app_name;
	}

	// create any directories we expect to write to.

	int res = LLFile::mkdir(mOSUserAppDir);
	if (res == -1)
	{
		LL_WARNS() << "Couldn't create app user dir " << mOSUserAppDir << LL_ENDL;
		LL_WARNS() << "Default to base dir" << mOSUserDir << LL_ENDL;
		mOSUserAppDir = mOSUserDir;
	}

	res = LLFile::mkdir(getExpandedFilename(LL_PATH_LOGS,""));
	if (res == -1)
	{
		LL_WARNS() << "Couldn't create LL_PATH_LOGS dir " << getExpandedFilename(LL_PATH_LOGS,"") << LL_ENDL;
	}
	
	res = LLFile::mkdir(getExpandedFilename(LL_PATH_USER_SETTINGS,""));
	if (res == -1)
	{
		LL_WARNS() << "Couldn't create LL_PATH_USER_SETTINGS dir " << getExpandedFilename(LL_PATH_USER_SETTINGS,"") << LL_ENDL;
	}

	res = LLFile::mkdir(getExpandedFilename(LL_PATH_CACHE,""));
	if (res == -1)
	{
		LL_WARNS() << "Couldn't create LL_PATH_CACHE dir " << getExpandedFilename(LL_PATH_CACHE,"") << LL_ENDL;
	}

	mCAFile = getExpandedFilename(LL_PATH_EXECUTABLE, "ca-bundle.crt");
}

U32 LLDir_Solaris::countFilesInDir(const std::string &dirname, const std::string &mask)
{
	U32 file_count = 0;
	glob_t g;

	std::string tmp_str;
	tmp_str = dirname;
	tmp_str += mask;
	
	if(glob(tmp_str.c_str(), GLOB_NOSORT, NULL, &g) == 0)
	{
		file_count = g.gl_pathc;

		globfree(&g);
	}

	return (file_count);
}

std::string LLDir_Solaris::getCurPath()
{
	char tmp_str[LL_MAX_PATH];	/* Flawfinder: ignore */ 
	if (getcwd(tmp_str, LL_MAX_PATH) == NULL)
	{
		LL_WARNS() << "Could not get current directory" << LL_ENDL;
		tmp_str[0] = '\0';
	}
	return tmp_str;
}


bool LLDir_Solaris::fileExists(const std::string &filename) const
{
	struct stat stat_data;
	// Check the age of the file
	// Now, we see if the files we've gathered are recent...
	int res = stat(filename.c_str(), &stat_data);
	if (!res)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

