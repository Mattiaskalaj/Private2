/** 
 * @file llpanelmarketplaceinbox.h
 * @brief Panel for marketplace inbox
 *
* $LicenseInfo:firstyear=2011&license=viewerlgpl$
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

#ifndef LL_LLPANELMARKETPLACEINBOX_H
#define LL_LLPANELMARKETPLACEINBOX_H

#include "llpanel.h"
#include "llfolderview.h"
class LLButton;
class LLInventoryPanel;
class LLUICtrl;

class LLPanelMarketplaceInbox : public LLPanel
{
public:

	struct Params :	public LLInitParam::Block<Params, LLPanel::Params>
	{};

	LOG_CLASS(LLPanelMarketplaceInbox);

	// RN: for some reason you can't just use LLUICtrlFactory::getDefaultParams as a default argument in VC8
	static const LLPanelMarketplaceInbox::Params& getDefaultParams();

	LLPanelMarketplaceInbox(const Params& p = getDefaultParams());
	~LLPanelMarketplaceInbox();

	/*virtual*/ BOOL postBuild();
	
	/*virtual*/ BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop, EDragAndDropType cargo_type, void *cargo_data, EAcceptance *accept, std::string& tooltip_msg);

	/*virtual*/ void draw();

private:

	void onSelectionChange();

	void onFocusReceived();
	LLSaveFolderState*			mSavedFolderState;
};


#endif //LL_LLPANELMARKETPLACEINBOX_H
