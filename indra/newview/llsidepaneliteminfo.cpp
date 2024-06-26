/** 
 * @file llsidepaneliteminfo.cpp
 * @brief A floater which shows an inventory item's properties.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"
#include "llsidepaneliteminfo.h"

#include "roles_constants.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "llgroupactions.h"
#include "llinventorydefines.h"
#include "llinventorymodel.h"
#include "llinventoryobserver.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "llslurl.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llexperiencecache.h"
#include "lltrans.h"
#include "llviewerregion.h"
// [RLVa:KB] - Checked: RLVa-2.0.1
#include "rlvactions.h"
#include "rlvcommon.h"
// [/RLVa:KB]

//BD
#include "llnamebox.h"

class PropertiesChangedCallback : public LLInventoryCallback
{
public:
    PropertiesChangedCallback(LLHandle<LLPanel> sidepanel_handle, LLUUID &item_id, S32 id)
        : mHandle(sidepanel_handle), mItemId(item_id), mId(id)
    {}

    void fire(const LLUUID &inv_item)
    {
        // inv_item can be null for some reason
        LLSidepanelItemInfo* sidepanel = dynamic_cast<LLSidepanelItemInfo*>(mHandle.get());
        if (sidepanel)
        {
            // sidepanel waits only for most recent update
            sidepanel->onUpdateCallback(mItemId, mId);
        }
    }
private:
    LLHandle<LLPanel> mHandle;
    LLUUID mItemId;
    S32 mId;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLItemPropertiesObserver
//
// Helper class to watch for changes to the item.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLItemPropertiesObserver : public LLInventoryObserver
{
public:
	LLItemPropertiesObserver(LLSidepanelItemInfo* floater)
		: mFloater(floater)
	{
		gInventory.addObserver(this);
	}
	virtual ~LLItemPropertiesObserver()
	{
		gInventory.removeObserver(this);
	}
	virtual void changed(U32 mask);
private:
	LLSidepanelItemInfo* mFloater; // Not a handle because LLSidepanelItemInfo is managing LLItemPropertiesObserver
};

void LLItemPropertiesObserver::changed(U32 mask)
{
	const std::set<LLUUID>& mChangedItemIDs = gInventory.getChangedIDs();
	std::set<LLUUID>::const_iterator it;

	const LLUUID& item_id = mFloater->getItemID();

	for (it = mChangedItemIDs.begin(); it != mChangedItemIDs.end(); it++)
	{
		// set dirty for 'item profile panel' only if changed item is the item for which 'item profile panel' is shown (STORM-288)
		if (*it == item_id)
		{
			// if there's a change we're interested in.
			if((mask & (LLInventoryObserver::LABEL | LLInventoryObserver::INTERNAL | LLInventoryObserver::REMOVE)) != 0)
			{
				mFloater->dirty();
			}
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLObjectInventoryObserver
//
// Helper class to watch for changes in an object inventory.
// Used to update item properties in LLSidepanelItemInfo.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLObjectInventoryObserver : public LLVOInventoryListener
{
public:
	LLObjectInventoryObserver(LLSidepanelItemInfo* floater, LLViewerObject* object)
		: mFloater(floater)
	{
		registerVOInventoryListener(object, NULL);
	}
	virtual ~LLObjectInventoryObserver()
	{
		removeVOInventoryListener();
	}
	/*virtual*/ void inventoryChanged(LLViewerObject* object,
									  LLInventoryObject::object_list_t* inventory,
									  S32 serial_num,
									  void* user_data);
private:
	LLSidepanelItemInfo* mFloater;  // Not a handle because LLSidepanelItemInfo is managing LLObjectInventoryObserver
};

/*virtual*/
void LLObjectInventoryObserver::inventoryChanged(LLViewerObject* object,
												 LLInventoryObject::object_list_t* inventory,
												 S32 serial_num,
												 void* user_data)
{
	mFloater->dirty();
}

///----------------------------------------------------------------------------
/// Class LLSidepanelItemInfo
///----------------------------------------------------------------------------

static LLPanelInjector<LLSidepanelItemInfo> t_item_info("sidepanel_item_info");

// Default constructor
LLSidepanelItemInfo::LLSidepanelItemInfo(const LLPanel::Params& p)
	: LLSidepanelInventorySubpanel(p)
	, mItemID(LLUUID::null)
	, mObjectInventoryObserver(NULL)
	, mUpdatePendingId(-1)
{
	mPropertiesObserver = new LLItemPropertiesObserver(this);
}

// Destroys the object
LLSidepanelItemInfo::~LLSidepanelItemInfo()
{
	delete mPropertiesObserver;
	mPropertiesObserver = NULL;

	stopObjectInventoryObserver();
}

// virtual
BOOL LLSidepanelItemInfo::postBuild()
{
	LLSidepanelInventorySubpanel::postBuild();

	mLabelItemNameTitle = getChild<LLUICtrl>("LabelItemNameTitle");
	mLabelItemName = getChild<LLLineEditor>("LabelItemName");
	mLabelItemDescTitle = getChild<LLUICtrl>("LabelItemDescTitle");
	mLabelItemDesc = getChild<LLLineEditor>("LabelItemDesc");
	mLabelCreatorTitle = getChild<LLUICtrl>("LabelCreatorTitle");
	mLabelCreatorName = getChild<LLUICtrl>("LabelCreatorName");
	mLabelOwnerTitle = getChild<LLUICtrl>("LabelOwnerTitle");
	mLabelOwnerName = getChild<LLUICtrl>("LabelOwnerName");

	mLabelItemName->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);
	mLabelItemName->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitName,this));
	mLabelItemDesc->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);
	mLabelItemDesc->setCommitCallback(boost::bind(&LLSidepanelItemInfo:: onCommitDescription, this));
	// Creator information
	getChild<LLUICtrl>("BtnCreator")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onClickCreator,this));
	// owner information
	getChild<LLUICtrl>("BtnOwner")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onClickOwner,this));
	// acquired date
	// owner permissions
	// Permissions debug text
	// group permissions
	getChild<LLUICtrl>("CheckShareWithGroup")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this, _1));
	// everyone permissions
	getChild<LLUICtrl>("CheckEveryoneCopy")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this, _1));
	// next owner permissions
	getChild<LLUICtrl>("CheckNextOwnerModify")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this, _1));
	getChild<LLUICtrl>("CheckNextOwnerCopy")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this, _1));
	getChild<LLUICtrl>("CheckNextOwnerTransfer")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this, _1));
	// Mark for sale or not, and sale info
	getChild<LLUICtrl>("CheckPurchase")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitSaleInfo, this, _1));
	// Change sale type, and sale info
	getChild<LLUICtrl>("ComboBoxSaleType")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitSaleInfo, this, _1));
	// "Price" label for edit
	getChild<LLUICtrl>("Edit Cost")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitSaleInfo, this, _1));
	refresh();
	return TRUE;
}

void LLSidepanelItemInfo::setObjectID(const LLUUID& object_id)
{
	mObjectID = object_id;

	// Start monitoring changes in the object inventory to update
	// selected inventory item properties in Item Profile panel. See STORM-148.
	startObjectInventoryObserver();
	mUpdatePendingId = -1;
}

void LLSidepanelItemInfo::setItemID(const LLUUID& item_id)
{
    if (mItemID != item_id)
    {
        mItemID = item_id;
        mUpdatePendingId = -1;
    }
}

const LLUUID& LLSidepanelItemInfo::getObjectID() const
{
	return mObjectID;
}

const LLUUID& LLSidepanelItemInfo::getItemID() const
{
	return mItemID;
}

void LLSidepanelItemInfo::onUpdateCallback(const LLUUID& item_id, S32 received_update_id)
{
    if (mItemID == item_id && mUpdatePendingId == received_update_id)
    {
        mUpdatePendingId = -1;
        refresh();
    }
}

void LLSidepanelItemInfo::reset()
{
	LLSidepanelInventorySubpanel::reset();

	mObjectID = LLUUID::null;
	mItemID = LLUUID::null;

	stopObjectInventoryObserver();
}

void LLSidepanelItemInfo::refresh()
{
	LLViewerInventoryItem* item = findItem();
	if(item)
	{
		refreshFromItem(item);
		updateVerbs();
		return;
	}
	else
	{
		if (getIsEditing())
		{
			setIsEditing(FALSE);
		}
	}

	if (!getIsEditing())
	{
		const std::string no_item_names[]={
			"LabelItemName",
			"LabelItemDesc",
			"LabelCreatorName",
			"LabelOwnerName"
		};

		for(size_t t=0; t<LL_ARRAY_SIZE(no_item_names); ++t)
		{
			getChildView(no_item_names[t])->setEnabled(false);
		}

		setPropertiesFieldsEnabled(false);

		const std::string hide_names[]={
			"BaseMaskDebug",
			"OwnerMaskDebug",
			"GroupMaskDebug",
			"EveryoneMaskDebug",
			"NextMaskDebug"
		};
		for(size_t t=0; t<LL_ARRAY_SIZE(hide_names); ++t)
		{
			getChildView(hide_names[t])->setVisible(false);
		}
	}

	if (!item)
	{
		const std::string no_edit_mode_names[]={
			"BtnCreator",
			"BtnOwner",
		};
		for(size_t t=0; t<LL_ARRAY_SIZE(no_edit_mode_names); ++t)
		{
			getChildView(no_edit_mode_names[t])->setEnabled(false);
		}
	}

	updateVerbs();
}

void LLSidepanelItemInfo::refreshFromItem(LLViewerInventoryItem* item)
{
	////////////////////////
	// PERMISSIONS LOOKUP //
	////////////////////////

	llassert(item);
	if (!item) return;

    if (mUpdatePendingId != -1)
    {
        return;
    }

	// do not enable the UI for incomplete items.
	BOOL is_complete = item->isFinished();
	const BOOL cannot_restrict_permissions = LLInventoryType::cannotRestrictPermissions(item->getInventoryType());
	const BOOL is_calling_card = (item->getInventoryType() == LLInventoryType::IT_CALLINGCARD);
	const LLPermissions& perm = item->getPermissions();
	const BOOL can_agent_manipulate = gAgent.allowOperation(PERM_OWNER, perm, 
								GP_OBJECT_MANIPULATE);
	const BOOL can_agent_sell = gAgent.allowOperation(PERM_OWNER, perm, 
							  GP_OBJECT_SET_SALE) &&
		!cannot_restrict_permissions;
	const BOOL is_link = item->getIsLinkType();
	
	const LLUUID trash_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
	bool not_in_trash = (item->getUUID() != trash_id) && !gInventory.isObjectDescendentOf(item->getUUID(), trash_id);

	// You need permission to modify the object to modify an inventory
	// item in it.
	LLViewerObject* object = NULL;
	if(!mObjectID.isNull()) object = gObjectList.findObject(mObjectID);
	BOOL is_obj_modify = TRUE;
	if(object)
	{
		is_obj_modify = object->permOwnerModify();
	}

    if(item->getInventoryType() == LLInventoryType::IT_LSL)
    {
        getChildView("LabelItemExperienceTitle")->setVisible(TRUE);
        LLTextBox* tb = getChild<LLTextBox>("LabelItemExperience");
        tb->setText(getString("loading_experience"));
        tb->setVisible(TRUE);
        std::string url = std::string();
        if(object && object->getRegion())
        {
            url = object->getRegion()->getCapability("GetMetadata");
        }
        LLExperienceCache::instance().fetchAssociatedExperience(item->getParentUUID(), item->getUUID(), url,
                boost::bind(&LLSidepanelItemInfo::setAssociatedExperience, getDerivedHandle<LLSidepanelItemInfo>(), _1));
    }
    
	//////////////////////
	// ITEM NAME & DESC //
	//////////////////////
	BOOL is_modifiable = gAgent.allowOperation(PERM_MODIFY, perm,
											   GP_OBJECT_MANIPULATE)
		&& is_obj_modify && is_complete && not_in_trash;

	mLabelItemNameTitle->setEnabled(TRUE);
	mLabelItemName->setEnabled(is_modifiable && !is_calling_card); // for now, don't allow rename of calling cards
	mLabelItemName->setValue(item->getName());
	mLabelItemDescTitle->setEnabled(TRUE);
	mLabelItemDesc->setEnabled(is_modifiable);
	getChildView("IconLocked")->setVisible(!is_modifiable);
	mLabelItemDesc->setValue(item->getDescription());

	//////////////////
	// CREATOR NAME //
	//////////////////
	if(!gCacheName) return;
	if(!gAgent.getRegion()) return;

	if (item->getCreatorUUID().notNull())
	{
		LLUUID creator_id = item->getCreatorUUID();
//		std::string name =
//			LLSLURL("agent", creator_id, "completename").getSLURLString();
//		getChildView("BtnCreator")->setEnabled(TRUE);
// [RLVa:KB] - Checked: RLVa-2.0.1
		// If the object creator matches the object owner we need to anonymize the creator field as well
		bool fRlvCanShowCreator = true;
		if ( (RlvActions::isRlvEnabled()) && (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, creator_id)) &&
		     ( ((perm.isOwned()) && (!perm.isGroupOwned()) && (perm.getOwner() == creator_id) ) || (RlvUtil::isNearbyAgent(item->getCreatorUUID())) ) )
		{
			fRlvCanShowCreator = false;
		}
		std::string name = LLSLURL("agent", creator_id, (fRlvCanShowCreator) ? "inspect" : "rlvanonym").getSLURLString();
		getChildView("BtnCreator")->setEnabled(fRlvCanShowCreator);
// [/RLVa:KB]
		mLabelCreatorTitle->setEnabled(TRUE);
		mLabelCreatorName->setValue(name);
//		//BD - SSFUI
		mLabelCreatorName->setEnabled(TRUE);
	}
	else
	{
		mLabelCreatorTitle->setEnabled(FALSE);
		mLabelCreatorName->setEnabled(FALSE);
		mLabelCreatorName->setValue(getString("unknown_multiple"));
	}

	////////////////
	// OWNER NAME //
	////////////////
	if(perm.isOwned())
	{
// [RLVa:KB] - Checked: RVLa-2.0.1
		bool fRlvCanShowOwner = true;
// [/RLVa:KB]
		std::string name;
		if (perm.isGroupOwned())
		{
//			//BD - SSFUI
			LLUUID group_owner_id = perm.getGroup();
			name = "secondlife:///app/group/" + group_owner_id.asString() + "/about";
		}
		else
		{
			LLUUID owner_id = perm.getOwner();
//			name = LLSLURL("agent", owner_id, "completename").getSLURLString();
// [RLVa:KB] - Checked: RLVa-2.0.1
			fRlvCanShowOwner = RlvActions::canShowName(RlvActions::SNC_DEFAULT, owner_id);
			name = LLSLURL("agent", owner_id, (fRlvCanShowOwner) ? "inspect" : "rlvanonym").getSLURLString();
// [/RLVa:KB]
		}
//		getChildView("BtnOwner")->setEnabled(TRUE);
// [RLVa:KB] - Checked: RLVa-2.0.1
		getChildView("BtnOwner")->setEnabled(fRlvCanShowOwner);
// [/RLVa:KB]
		mLabelOwnerTitle->setEnabled(TRUE);
		mLabelOwnerName->setValue(name);
//		//BD - SSFUI
		mLabelOwnerName->setEnabled(TRUE);
	}
	else
	{
		mLabelOwnerTitle->setEnabled(FALSE);
		mLabelOwnerName->setEnabled(FALSE);
		mLabelOwnerName->setValue(getString("public"));
	}
	
	////////////
	// ORIGIN //
	////////////

	if (object)
	{
		getChild<LLUICtrl>("origin")->setValue(getString("origin_inworld"));
	}
	else
	{
		getChild<LLUICtrl>("origin")->setValue(getString("origin_inventory"));
	}

	//////////////////
	// ACQUIRE DATE //
	//////////////////
	
	time_t time_utc = item->getCreationDate();
	if (0 == time_utc)
	{
		getChild<LLUICtrl>("LabelAcquiredDate")->setValue(getString("unknown"));
	}
	else
	{
		std::string timeStr = getString("acquiredDate");
		LLSD substitution;
		substitution["datetime"] = (S32) time_utc;
		LLStringUtil::format (timeStr, substitution);
		getChild<LLUICtrl>("LabelAcquiredDate")->setValue(timeStr);
	}
	
	//////////////////////////////////////
	// PERMISSIONS AND SALE ITEM HIDING //
	//////////////////////////////////////
	
	const std::string perm_and_sale_items[]={
		"perms_inv",
		"perm_modify",
		"CheckOwnerModify",
		"CheckOwnerCopy",
		"CheckOwnerTransfer",
		"GroupLabel",
		"CheckShareWithGroup",
		"AnyoneLabel",
		"CheckEveryoneCopy",
		"NextOwnerLabel",
		"CheckNextOwnerModify",
		"CheckNextOwnerCopy",
		"CheckNextOwnerTransfer",
		"CheckPurchase",
		"ComboBoxSaleType",
		"Edit Cost"
	};
	
	const std::string debug_items[]={
		"BaseMaskDebug",
		"OwnerMaskDebug",
		"GroupMaskDebug",
		"EveryoneMaskDebug",
		"NextMaskDebug"
	};
	
	// Hide permissions checkboxes and labels and for sale info if in the trash
	// or ui elements don't apply to these objects and return from function
	if (!not_in_trash || cannot_restrict_permissions)
	{
		for(size_t t=0; t<LL_ARRAY_SIZE(perm_and_sale_items); ++t)
		{
			getChildView(perm_and_sale_items[t])->setVisible(false);
		}
		
		for(size_t t=0; t<LL_ARRAY_SIZE(debug_items); ++t)
		{
			getChildView(debug_items[t])->setVisible(false);
		}
		return;
	}
	else // Make sure perms and sale ui elements are visible
	{
		for(size_t t=0; t<LL_ARRAY_SIZE(perm_and_sale_items); ++t)
		{
			getChildView(perm_and_sale_items[t])->setVisible(true);
		}
	}

	///////////////////////
	// OWNER PERMISSIONS //
	///////////////////////

	U32 base_mask		= perm.getMaskBase();
	U32 owner_mask		= perm.getMaskOwner();
	U32 group_mask		= perm.getMaskGroup();
	U32 everyone_mask	= perm.getMaskEveryone();
	U32 next_owner_mask	= perm.getMaskNextOwner();

	getChildView("CheckOwnerModify")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerModify")->setValue(LLSD((BOOL)(owner_mask & PERM_MODIFY)));
	getChildView("CheckOwnerCopy")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerCopy")->setValue(LLSD((BOOL)(owner_mask & PERM_COPY)));
	getChildView("CheckOwnerTransfer")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerTransfer")->setValue(LLSD((BOOL)(owner_mask & PERM_TRANSFER)));

	///////////////////////
	// DEBUG PERMISSIONS //
	///////////////////////

	if( gSavedSettings.getBOOL("DebugPermissions") )
	{
		BOOL slam_perm 			= FALSE;
		BOOL overwrite_group	= FALSE;
		BOOL overwrite_everyone	= FALSE;

		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = item->getFlags();
			slam_perm 			= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
			overwrite_everyone	= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
			overwrite_group		= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
		}
		
		std::string perm_string;

		perm_string = "B: ";
		perm_string += mask_to_string(base_mask);
		getChild<LLUICtrl>("BaseMaskDebug")->setValue(perm_string);
		getChildView("BaseMaskDebug")->setVisible(TRUE);
		
		perm_string = "O: ";
		perm_string += mask_to_string(owner_mask);
		getChild<LLUICtrl>("OwnerMaskDebug")->setValue(perm_string);
		getChildView("OwnerMaskDebug")->setVisible(TRUE);
		
		perm_string = "G";
		perm_string += overwrite_group ? "*: " : ": ";
		perm_string += mask_to_string(group_mask);
		getChild<LLUICtrl>("GroupMaskDebug")->setValue(perm_string);
		getChildView("GroupMaskDebug")->setVisible(TRUE);
		
		perm_string = "E";
		perm_string += overwrite_everyone ? "*: " : ": ";
		perm_string += mask_to_string(everyone_mask);
		getChild<LLUICtrl>("EveryoneMaskDebug")->setValue(perm_string);
		getChildView("EveryoneMaskDebug")->setVisible(TRUE);
		
		perm_string = "N";
		perm_string += slam_perm ? "*: " : ": ";
		perm_string += mask_to_string(next_owner_mask);
		getChild<LLUICtrl>("NextMaskDebug")->setValue(perm_string);
		getChildView("NextMaskDebug")->setVisible(TRUE);
	}
	else
	{
		getChildView("BaseMaskDebug")->setVisible(FALSE);
		getChildView("OwnerMaskDebug")->setVisible(FALSE);
		getChildView("GroupMaskDebug")->setVisible(FALSE);
		getChildView("EveryoneMaskDebug")->setVisible(FALSE);
		getChildView("NextMaskDebug")->setVisible(FALSE);
	}

	/////////////
	// SHARING //
	/////////////

	// Check for ability to change values.
	if (is_link || cannot_restrict_permissions)
	{
		getChildView("CheckShareWithGroup")->setEnabled(FALSE);
		getChildView("CheckEveryoneCopy")->setEnabled(FALSE);
	}
	else if (is_obj_modify && can_agent_manipulate)
	{
		getChildView("CheckShareWithGroup")->setEnabled(TRUE);
		getChildView("CheckEveryoneCopy")->setEnabled((owner_mask & PERM_COPY) && (owner_mask & PERM_TRANSFER));
	}
	else
	{
		getChildView("CheckShareWithGroup")->setEnabled(FALSE);
		getChildView("CheckEveryoneCopy")->setEnabled(FALSE);
	}

	// Set values.
	BOOL is_group_copy = (group_mask & PERM_COPY) ? TRUE : FALSE;
	BOOL is_group_modify = (group_mask & PERM_MODIFY) ? TRUE : FALSE;
	BOOL is_group_move = (group_mask & PERM_MOVE) ? TRUE : FALSE;

	if (is_group_copy && is_group_modify && is_group_move)
	{
		getChild<LLUICtrl>("CheckShareWithGroup")->setValue(LLSD((BOOL)TRUE));

		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(FALSE);
		}
	}
	else if (!is_group_copy && !is_group_modify && !is_group_move)
	{
		getChild<LLUICtrl>("CheckShareWithGroup")->setValue(LLSD((BOOL)FALSE));
		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(FALSE);
		}
	}
	else
	{
		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(!ctl->getEnabled());
			ctl->set(TRUE);
		}
	}
	
	getChild<LLUICtrl>("CheckEveryoneCopy")->setValue(LLSD((BOOL)(everyone_mask & PERM_COPY)));

	///////////////
	// SALE INFO //
	///////////////

	const LLSaleInfo& sale_info = item->getSaleInfo();
	BOOL is_for_sale = sale_info.isForSale();
	LLComboBox* combo_sale_type = getChild<LLComboBox>("ComboBoxSaleType");
	LLUICtrl* edit_cost = getChild<LLUICtrl>("Edit Cost");

	// Check for ability to change values.
	if (is_obj_modify && can_agent_sell 
		&& gAgent.allowOperation(PERM_TRANSFER, perm, GP_OBJECT_MANIPULATE))
	{
		getChildView("CheckPurchase")->setEnabled(is_complete);

		getChildView("NextOwnerLabel")->setEnabled(TRUE);
		getChildView("CheckNextOwnerModify")->setEnabled((base_mask & PERM_MODIFY) && !cannot_restrict_permissions);
		getChildView("CheckNextOwnerCopy")->setEnabled((base_mask & PERM_COPY) && !cannot_restrict_permissions);
		getChildView("CheckNextOwnerTransfer")->setEnabled((next_owner_mask & PERM_COPY) && !cannot_restrict_permissions);

		combo_sale_type->setEnabled(is_complete && is_for_sale);
		edit_cost->setEnabled(is_complete && is_for_sale);
	}
	else
	{
		getChildView("CheckPurchase")->setEnabled(FALSE);

		getChildView("NextOwnerLabel")->setEnabled(FALSE);
		getChildView("CheckNextOwnerModify")->setEnabled(FALSE);
		getChildView("CheckNextOwnerCopy")->setEnabled(FALSE);
		getChildView("CheckNextOwnerTransfer")->setEnabled(FALSE);

		combo_sale_type->setEnabled(FALSE);
		edit_cost->setEnabled(FALSE);
	}

	// Set values.
	getChild<LLUICtrl>("CheckPurchase")->setValue(is_for_sale);
	getChild<LLUICtrl>("CheckNextOwnerModify")->setValue(LLSD(BOOL(next_owner_mask & PERM_MODIFY)));
	getChild<LLUICtrl>("CheckNextOwnerCopy")->setValue(LLSD(BOOL(next_owner_mask & PERM_COPY)));
	getChild<LLUICtrl>("CheckNextOwnerTransfer")->setValue(LLSD(BOOL(next_owner_mask & PERM_TRANSFER)));

	if (is_for_sale)
	{
		S32 numerical_price;
		numerical_price = sale_info.getSalePrice();
		edit_cost->setValue(llformat("%d",numerical_price));
		combo_sale_type->setValue(sale_info.getSaleType());
	}
	else
	{
		edit_cost->setValue(llformat("%d",0));
		combo_sale_type->setValue(LLSaleInfo::FS_COPY);
	}
}


void LLSidepanelItemInfo::setAssociatedExperience( LLHandle<LLSidepanelItemInfo> hInfo, const LLSD& experience )
{
    LLSidepanelItemInfo* info = hInfo.get();
    if(info)
    {
        LLUUID id;
        if(experience.has(LLExperienceCache::EXPERIENCE_ID))
        {
            id=experience[LLExperienceCache::EXPERIENCE_ID].asUUID();
        }
        if(id.notNull())
        {
            info->getChild<LLTextBox>("LabelItemExperience")->setText(LLSLURL("experience", id, "profile").getSLURLString());    
        }
        else
        {
            info->getChild<LLTextBox>("LabelItemExperience")->setText(LLTrans::getString("ExperienceNameNull"));
        }
    }
}


void LLSidepanelItemInfo::startObjectInventoryObserver()
{
	if (!mObjectInventoryObserver)
	{
		stopObjectInventoryObserver();

		// Previous object observer should be removed before starting to observe a new object.
		llassert(mObjectInventoryObserver == NULL);
	}

	if (mObjectID.isNull())
	{
		LL_WARNS() << "Empty object id passed to inventory observer" << LL_ENDL;
		return;
	}

	LLViewerObject* object = gObjectList.findObject(mObjectID);

	mObjectInventoryObserver = new LLObjectInventoryObserver(this, object);
}

void LLSidepanelItemInfo::stopObjectInventoryObserver()
{
	delete mObjectInventoryObserver;
	mObjectInventoryObserver = NULL;
}

void LLSidepanelItemInfo::setPropertiesFieldsEnabled(bool enabled)
{
    const std::string fields[] = {
        "CheckOwnerModify",
        "CheckOwnerCopy",
        "CheckOwnerTransfer",
        "CheckShareWithGroup",
        "CheckEveryoneCopy",
        "CheckNextOwnerModify",
        "CheckNextOwnerCopy",
        "CheckNextOwnerTransfer",
        "CheckPurchase",
        "Edit Cost"
    };
    for (size_t t = 0; t<LL_ARRAY_SIZE(fields); ++t)
    {
        getChildView(fields[t])->setEnabled(false);
    }
}

void LLSidepanelItemInfo::onClickCreator()
{
	LLViewerInventoryItem* item = findItem();
	if(!item) return;
	if(!item->getCreatorUUID().isNull())
	{
// [RLVa:KB] - Checked: RLVa-1.2.1
		const LLUUID& idCreator = item->getCreatorUUID();
		if ( (RlvActions::isRlvEnabled()) && (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, idCreator)) )
		{
			const LLPermissions& perm = item->getPermissions();
			if ( ((perm.isOwned()) && (!perm.isGroupOwned()) && (perm.getOwner() == idCreator) ) || (RlvUtil::isNearbyAgent(idCreator)) )
			{
				return;
			}
		}
// [/RLVa:KB]
		LLAvatarActions::showProfile(item->getCreatorUUID());
	}
}

// static
void LLSidepanelItemInfo::onClickOwner()
{
	LLViewerInventoryItem* item = findItem();
	if(!item) return;
	if(item->getPermissions().isGroupOwned())
	{
		LLGroupActions::show(item->getPermissions().getGroup());
	}
	else
	{
// [RLVa:KB] - Checked: RLVa-1.0.0
		if ( (RlvActions::isRlvEnabled()) && (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, item->getPermissions().getOwner())) )
			return;
// [/RLVa:KB]
		LLAvatarActions::showProfile(item->getPermissions().getOwner());
	}
}

// static
void LLSidepanelItemInfo::onCommitName()
{
	//LL_INFOS() << "LLSidepanelItemInfo::onCommitName()" << LL_ENDL;
	LLViewerInventoryItem* item = findItem();
	if(!item)
	{
		return;
	}

	if(mLabelItemName &&
	   (item->getName() != mLabelItemName->getText()) &&
	   (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE)) )
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->rename(mLabelItemName->getText());
		onCommitChanges(new_item);
	}
}

void LLSidepanelItemInfo::onCommitDescription()
{
	//LL_INFOS() << "LLSidepanelItemInfo::onCommitDescription()" << LL_ENDL;
	LLViewerInventoryItem* item = findItem();
	if(!item) return;

	if((item->getDescription() != mLabelItemDesc->getText()) &&
	   (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE)))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		new_item->setDescription(mLabelItemDesc->getText());
		onCommitChanges(new_item);
	}
}

void LLSidepanelItemInfo::onCommitPermissions(LLUICtrl* ctrl)
{
    if (ctrl)
    {
        // will be enabled by response from server
        ctrl->setEnabled(false);
    }
    updatePermissions();
}

void LLSidepanelItemInfo::updatePermissions()
{
	LLViewerInventoryItem* item = findItem();
	if(!item) return;

	BOOL is_group_owned;
	LLUUID owner_id;
	LLUUID group_id;
	LLPermissions perm(item->getPermissions());
	perm.getOwnership(owner_id, is_group_owned);

	if (is_group_owned && gAgent.hasPowerInGroup(owner_id, GP_OBJECT_MANIPULATE))
	{
		group_id = owner_id;
	}

	LLCheckBoxCtrl* CheckShareWithGroup = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");

	if(CheckShareWithGroup)
	{
		perm.setGroupBits(gAgent.getID(), group_id,
						CheckShareWithGroup->get(),
						PERM_MODIFY | PERM_MOVE | PERM_COPY);
	}
	LLCheckBoxCtrl* CheckEveryoneCopy = getChild<LLCheckBoxCtrl>("CheckEveryoneCopy");
	if(CheckEveryoneCopy)
	{
		perm.setEveryoneBits(gAgent.getID(), group_id,
						 CheckEveryoneCopy->get(), PERM_COPY);
	}

	LLCheckBoxCtrl* CheckNextOwnerModify = getChild<LLCheckBoxCtrl>("CheckNextOwnerModify");
	if(CheckNextOwnerModify)
	{
		perm.setNextOwnerBits(gAgent.getID(), group_id,
							CheckNextOwnerModify->get(), PERM_MODIFY);
	}
	LLCheckBoxCtrl* CheckNextOwnerCopy = getChild<LLCheckBoxCtrl>("CheckNextOwnerCopy");
	if(CheckNextOwnerCopy)
	{
		perm.setNextOwnerBits(gAgent.getID(), group_id,
							CheckNextOwnerCopy->get(), PERM_COPY);
	}
	LLCheckBoxCtrl* CheckNextOwnerTransfer = getChild<LLCheckBoxCtrl>("CheckNextOwnerTransfer");
	if(CheckNextOwnerTransfer)
	{
		perm.setNextOwnerBits(gAgent.getID(), group_id,
							CheckNextOwnerTransfer->get(), PERM_TRANSFER);
	}
	if(perm != item->getPermissions()
		&& item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setPermissions(perm);
		U32 flags = new_item->getFlags();
		// If next owner permissions have changed (and this is an object)
		// then set the slam permissions flag so that they are applied on rez.
		if((perm.getMaskNextOwner()!=item->getPermissions().getMaskNextOwner())
		   && (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
		}
		// If everyone permissions have changed (and this is an object)
		// then set the overwrite everyone permissions flag so they
		// are applied on rez.
		if ((perm.getMaskEveryone()!=item->getPermissions().getMaskEveryone())
			&& (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
		}
		// If group permissions have changed (and this is an object)
		// then set the overwrite group permissions flag so they
		// are applied on rez.
		if ((perm.getMaskGroup()!=item->getPermissions().getMaskGroup())
			&& (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
		}
		new_item->setFlags(flags);
		onCommitChanges(new_item);
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

// static
void LLSidepanelItemInfo::onCommitSaleInfo(LLUICtrl* ctrl)
{
    if (ctrl)
    {
        // will be enabled by response from server
        ctrl->setEnabled(false);
    }
	//LL_INFOS() << "LLSidepanelItemInfo::onCommitSaleInfo()" << LL_ENDL;
	updateSaleInfo();
}

void LLSidepanelItemInfo::updateSaleInfo()
{
	LLViewerInventoryItem* item = findItem();
	if(!item) return;
	LLSaleInfo sale_info(item->getSaleInfo());
	if(!gAgent.allowOperation(PERM_TRANSFER, item->getPermissions(), GP_OBJECT_SET_SALE))
	{
		getChild<LLUICtrl>("CheckPurchase")->setValue(LLSD((BOOL)FALSE));
	}

	if((BOOL)getChild<LLUICtrl>("CheckPurchase")->getValue())
	{
		// turn on sale info
		LLSaleInfo::EForSale sale_type = LLSaleInfo::FS_COPY;
	
		LLComboBox* combo_sale_type = getChild<LLComboBox>("ComboBoxSaleType");
		if (combo_sale_type)
		{
			sale_type = static_cast<LLSaleInfo::EForSale>(combo_sale_type->getValue().asInteger());
		}

		if (sale_type == LLSaleInfo::FS_COPY 
			&& !gAgent.allowOperation(PERM_COPY, item->getPermissions(), 
									  GP_OBJECT_SET_SALE))
		{
			sale_type = LLSaleInfo::FS_ORIGINAL;
		}

	     
		
		S32 price = -1;
		price =  getChild<LLUICtrl>("Edit Cost")->getValue().asInteger();;

		// Invalid data - turn off the sale
		if (price < 0)
		{
			sale_type = LLSaleInfo::FS_NOT;
			price = 0;
		}

		sale_info.setSaleType(sale_type);
		sale_info.setSalePrice(price);
	}
	else
	{
		sale_info.setSaleType(LLSaleInfo::FS_NOT);
	}
	if(sale_info != item->getSaleInfo()
		&& item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		// Force an update on the sale price at rez
		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = new_item->getFlags();
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_SALE;
			new_item->setFlags(flags);
		}

		new_item->setSaleInfo(sale_info);
		onCommitChanges(new_item);
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

void LLSidepanelItemInfo::onCommitChanges(LLPointer<LLViewerInventoryItem> item)
{
    if (item.isNull())
    {
        return;
    }

    if (mObjectID.isNull())
    {
        // This is in the agent's inventory.
        // Mark update as pending and wait only for most recent one in case user requested for couple
        // Once update arrives or any of ids change drop pending id.
        mUpdatePendingId++;
        LLPointer<LLInventoryCallback> callback = new PropertiesChangedCallback(getHandle(), mItemID, mUpdatePendingId);
        update_inventory_item(item.get(), callback);
        //item->updateServer(FALSE);
        gInventory.updateItem(item);
        gInventory.notifyObservers();
    }
    else
    {
        // This is in an object's contents.
        LLViewerObject* object = gObjectList.findObject(mObjectID);
        if (object)
        {
            object->updateInventory(
                item,
                TASK_INVENTORY_ITEM_KEY,
                false);

            if (object->isSelected())
            {
                // Since object is selected (build floater is open) object will
                // receive properties update, detect serial mismatch, dirty and
                // reload inventory, meanwhile some other updates will refresh it.
                // So mark dirty early, this will prevent unnecessary changes
                // and download will be triggered by LLPanelObjectInventory - it
                // prevents flashing in content tab and some duplicated request.
                object->dirtyInventory();
            }
            setPropertiesFieldsEnabled(false);
        }
    }
}

LLViewerInventoryItem* LLSidepanelItemInfo::findItem() const
{
	LLViewerInventoryItem* item = NULL;
	if(mObjectID.isNull())
	{
		// it is in agent inventory
		item = gInventory.getItem(mItemID);
	}
	else
	{
		LLViewerObject* object = gObjectList.findObject(mObjectID);
		if(object)
		{
			item = static_cast<LLViewerInventoryItem*>(object->getInventoryObject(mItemID));
		}
	}
	return item;
}

// virtual
void LLSidepanelItemInfo::save()
{
	onCommitName();
	onCommitDescription();
	updatePermissions();
	updateSaleInfo();
}
