/**
 * @file llviewerinput.h
 * @brief LLViewerInput class header file
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

#ifndef LL_LLVIEWERINPUT_H
#define LL_LLVIEWERINPUT_H

#include "llkeyboard.h" // For EKeystate
#include "llinitparam.h"

const S32 MAX_KEY_BINDINGS = 128; // was 60
const S32 keybindings_xml_version = 1;
const std::string script_mouse_handler_name = "script_trigger_lbutton";

class LLNamedFunction
{
public:
	LLNamedFunction() : mFunction(NULL) { };
	~LLNamedFunction() { };

	std::string	mName;
	LLKeyFunc	mFunction;
};

class LLKeyboardBinding
{
public:
	KEY				mKey;
	EMouseClickType	mMouse;
	MASK			mMask;

	LLKeyFunc		mFunction;
};

class LLMouseBinding
{
public:
	EMouseClickType	mMouse;
	MASK			mMask;

	LLKeyFunc		mFunction;
};


typedef enum e_keyboard_mode
{
	MODE_FIRST_PERSON,
	MODE_THIRD_PERSON,
	MODE_EDIT,
	MODE_EDIT_AVATAR,
	MODE_SITTING,
	MODE_COUNT
} EKeyboardMode;

class LLWindow;

void bind_keyboard_functions();

class LLViewerInput
{
public:
	struct KeyBinding : public LLInitParam::Block<KeyBinding>
	{
		Mandatory<std::string>	key,
			mask,
			command;
		Optional<std::string>	mouse; // Note, not mandatory for the sake of backward campatibility with keys.xml

		KeyBinding();
	};

	struct KeyMode : public LLInitParam::Block<KeyMode>
	{
		Multiple<KeyBinding>		bindings;

		KeyMode();
	};

	struct Keys : public LLInitParam::Block<Keys>
	{
		Optional<KeyMode>	first_person,
			third_person,
			sitting,
			//							//BD - Custom Keyboard Layout
			editing,
			edit_avatar;
		Keys();
	};

	LLViewerInput();

	BOOL			handleKey(KEY key, MASK mask, BOOL repeated);
	BOOL			handleKeyUp(KEY key, MASK mask);

	EKeyboardMode	getMode() const;

	static BOOL		modeFromString(const std::string& string, S32 *mode);			// False on failure
	static BOOL		mouseFromString(const std::string& string, EMouseClickType *mode, bool translate = false);// False on failure

//	//BD - Custom Keyboard Layout
	std::string		stringFromMouse(EMouseClickType click, bool translate);

	// handleMouse() records state, scanMouse() goes through states, scanMouse(click) processes individual saved states after UI is done with them
	BOOL            handleMouse(LLWindow *window_impl, LLCoordGL pos, MASK mask, EMouseClickType clicktype, BOOL down);
	void            scanMouse();

    bool            isMouseBindUsed(const EMouseClickType mouse, const MASK mask, const S32 mode) const;
    bool            isLMouseHandlingDefault(const S32 mode) const { return mLMouseDefaultHandling[mode]; }

	//	//BD - Custom Keyboard Layout
	BOOL			exportBindingsXML(const std::string& filename);
	BOOL			unbindAllKeys(bool reset);
	BOOL			unbindModeKeys(bool reset, S32 mode);
	S32				loadBindingsSettings(const std::string& filename);

	bool            scanKey(KEY key, BOOL key_down, BOOL key_up, BOOL key_level);

	BOOL			bindControl(const S32 mode, const KEY key, const EMouseClickType mouse, const MASK mask, const std::string& function_name);
	BOOL			bindMouse(const S32 mode, const EMouseClickType mouse, const MASK mask, const std::string& function_name);
private:
	bool            scanKey(const std::vector<LLKeyboardBinding> &binding,
		S32 binding_count,
		KEY key,
		MASK mask,
		BOOL key_down,
		BOOL key_up,
		BOOL key_level,
		bool repeat) const;

	enum EMouseState
	{
		MOUSE_STATE_DOWN, // key down this frame
		MOUSE_STATE_CLICK, // key went up and down in scope of same frame
		MOUSE_STATE_LEVEL, // clicked again fast, or never released
		MOUSE_STATE_UP, // went up this frame
		MOUSE_STATE_SILENT // notified about 'up', do not notify again
	};
	bool			scanMouse(EMouseClickType click, EMouseState state) const;
	bool            scanMouse(EMouseClickType mouse, S32 mode, MASK mask, EMouseState state) const;

	void			resetBindings();

	// Hold all the ugly stuff torn out to make LLKeyboard non-viewer-specific here

    // TODO: at some point it is better to remake this, especially keyaboard part
    // would be much better to send to functions actual state of the button than
    // to send what we think function wants based on collection of bools (mKeyRepeated, mKeyLevel, mKeyDown)
    std::vector<LLKeyboardBinding>	mKeyBindings[MODE_COUNT];
    std::vector<LLMouseBinding>		mMouseBindings[MODE_COUNT];
    bool							mLMouseDefaultHandling[MODE_COUNT]; // Due to having special priority

	//	//BD - Custom Keyboard Layout
	S32				mBindingCount[MODE_COUNT];
	LLKeyBinding	mBindings[MODE_COUNT][MAX_KEY_BINDINGS];

	typedef std::map<U32, U32> key_remap_t;
	key_remap_t		mRemapKeys[MODE_COUNT];
	std::set<KEY>	mKeysSkippedByUI;
	BOOL			mKeyHandledByUI[KEY_COUNT];		// key processed successfully by UI

	// This is indentical to what llkeyboard does (mKeyRepeated, mKeyLevel, mKeyDown e t c),
	// just instead of remembering individually as bools,  we record state as enum
	EMouseState		mMouseLevel[CLICK_COUNT];	// records of key state
};

extern LLViewerInput gViewerInput;

#endif // LL_LLVIEWERINPUT_H