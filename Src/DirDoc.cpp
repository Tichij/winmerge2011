/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirDoc.cpp
 *
 * @brief Implementation file for CDirDoc
 *
 */
#include "StdAfx.h"
#include "Merge.h"
#include "ChildFrm.h"
#include "LanguageSelect.h"
#include "HexMergeFrm.h"
#include "ImgMergeFrm.h"
#include "SQLiteMergeFrm.h"
#include "ReoGridMergeFrm.h"
#include "7zCommon.h"
#include "DirFrame.h"
#include "DirView.h"
#include "MainFrm.h"
#include "coretools.h"
#include "markdown.h"
#include "LogFile.h"
#include "paths.h"
#include "FileActionScript.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/**
 * @brief Checks if there are mergedocs associated with this dirdoc.
 */
bool CDirFrame::CanFrameClose()
{
	return m_MergeDocs.empty()
		&& m_HexMergeDocs.empty()
		&& m_ImgMergeDocs.empty()
		&& m_SQLiteMergeDocs.empty()
		&& m_ReoGridMergeDocs.empty();
}

/////////////////////////////////////////////////////////////////////////////
// CDirDoc commands

bool CDirFrame::InitContext(LPCTSTR pszLeft, LPCTSTR pszRight, int nRecursive, DWORD dwContext)
{
	if (!COptionsMgr::Get(OPT_CMP_CACHE_RESULTS))
		DeleteContext();

	bool bNeedCompare = false;
	m_pCtxt = NULL;
	LIST_ENTRY *entry = &m_root;
	while ((entry = entry->Flink) != &m_root)
	{
		CDiffContext *pCtxt = static_cast<CDiffContext *>(entry);
		if (pCtxt->m_dwContext == dwContext && pCtxt->GetLeftPath() == pszLeft && pCtxt->GetRightPath() == pszRight)
		{
			m_pCtxt = pCtxt;
			break;
		}
	}
	if (m_pCtxt == NULL)
	{
		m_pCtxt = new CDiffContext(m_pCompareStats, m_pDirView->m_pWnd, pszLeft, pszRight, nRecursive, dwContext);
		m_root.Append(m_pCtxt);
		bNeedCompare = true;
	}
	return bNeedCompare;
}

/**
 * @brief Initialise directory compare for given paths.
 *
 * Initialises directory compare with paths given and recursive choice.
 * Previous compare context is first free'd.
 * @param [in] paths Paths to compare
 * @param [in] nRecursive If != 0 subdirectories are included to compare.
 */
bool CDirFrame::InitCompare(LPCTSTR pszLeft, LPCTSTR pszRight, int nRecursive, const MergeCmdLineInfo *cmdInfo, CTempPathContext *pTempPathContext)
{
	m_pDirView->DeleteAllItems();

	if (m_pCtxt && pTempPathContext)
	{
		pTempPathContext->m_strLeftParent = m_pCtxt->GetLeftPath();
		pTempPathContext->m_strRightParent = m_pCtxt->GetRightPath();
	}

	bool bNeedCompare = InitContext(pszLeft, pszRight, nRecursive, 0);

	if (pTempPathContext)
	{
		ApplyDisplayRoot(pTempPathContext->m_strLeftDisplayRoot);
		ApplyDisplayRoot(pTempPathContext->m_strRightDisplayRoot);
		pTempPathContext->m_pParent = m_pTempPathContext;
		m_pTempPathContext = pTempPathContext;
		m_pTempPathContext->m_strLeftRoot = m_pCtxt->GetLeftPath();
		m_pTempPathContext->m_strRightRoot = m_pCtxt->GetRightPath();
	}

	m_nRecursive = nRecursive;

	if (cmdInfo)
	{
		m_nCompMethod = cmdInfo->m_nCompMethod;
	}

	return bNeedCompare;
}

void CDirFrame::InitMrgmanCompare()
{
	if (m_nRecursive == 0)
	{
		m_wndFilePathBar.SendDlgItemMessage(
			IDC_STATIC_TITLE_LEFT, EM_SETLIMITTEXT, ID_MRGMAN_BASE);
		m_wndFilePathBar.SendDlgItemMessage(
			IDC_STATIC_TITLE_RIGHT, EM_SETLIMITTEXT, ID_MRGMAN_DESTINATION);
		m_nRecursive = 3;
	}
	else
	{
		m_pDirView->DeleteAllItems();
	}

	UINT idLeftContent = static_cast<UINT>(
		m_wndFilePathBar.SendDlgItemMessage(IDC_STATIC_TITLE_LEFT, EM_GETLIMITTEXT));
	UINT idRightContent = static_cast<UINT>(
		m_wndFilePathBar.SendDlgItemMessage(IDC_STATIC_TITLE_RIGHT, EM_GETLIMITTEXT));

	SetLeftReadOnly(idLeftContent != ID_MRGMAN_DESTINATION);
	SetRightReadOnly(idRightContent != ID_MRGMAN_DESTINATION);

	String mrgmanFile;
	GetWindowText(mrgmanFile);

	if (HMenu *const pMenu = LanguageSelect.LoadMenu(IDR_POPUP_EDITOR_HEADERBAR))
	{
		HMenu *const pSub = pMenu->GetSubMenu(1);
		TCHAR text[INFOTIPSIZE];
		pSub->GetMenuString(idLeftContent, text, _countof(text));
		m_wndFilePathBar.SetText(0, text, FALSE);
		pSub->GetMenuString(idRightContent, text, _countof(text));
		m_wndFilePathBar.SetText(1, text, FALSE);
		pMenu->DestroyMenu();
	}

	String root;
	CRegKeyEx rk;
	if (ERROR_SUCCESS == rk.OpenWithAccess(HKEY_LOCAL_MACHINE,
		_T("SYSTEM\\ControlSet001\\Services\\Mvfs\\Parameters"), KEY_READ))
	{
		root = rk.ReadString(_T("drive"), _T("?"));
		root += _T(":\\");
		root = paths_GetLongPath(root.c_str());
	}

	CMarkdown::File xml(mrgmanFile.c_str());
	CMarkdown::EntityMap entities;
	CMarkdown::Load(entities);

	bool bNeedCompare = false;
	if (xml.Move("cfl") && xml.Pull())
	{
		if (xml.Move("session") && xml.Pull())
		{
			OString view_root = CMarkdown(xml).Move("to-view").Pop().Move("view-root").GetInnerText()->Uni(entities);
			root += view_root.W;
			bNeedCompare = InitContext(root.c_str(), root.c_str(), 0, MAKELONG(idLeftContent, idRightContent));
			m_pCtxt->m_piFilterGlobal = &transparentFileFilter;
			if (bNeedCompare && xml.Move("files") && xml.Pull())
			{
				while (xml.Move("file") && xml.Pull())
				{
					String file_type;
					if (HString *hstr = CMarkdown(xml).Move("file-type").GetInnerText()->Uni(entities))
					{
						file_type = hstr->W;
						hstr->Free();
					}
					if (file_type == _T("File"))
					{
						m_pCompareStats->IncreaseTotalItems();
						String path;
						if (HString *hstr = CMarkdown(xml).Move("path").GetInnerText()->Uni(entities))
						{
							path = hstr->W;
							string_replace(path, _T('/'), _T('\\'));
							hstr->Free();
						}
						String base_version = _T("@@");
						if (HString *hstr = CMarkdown(xml).Move("base-version").Pop().Move("selector").GetInnerText()->Uni(entities))
						{
							base_version += hstr->W;
							string_replace(base_version, _T('/'), _T('\\'));
							hstr->Free();
						}
						String from_version = _T("@@");
						if (HString *hstr = CMarkdown(xml).Move("from-version").Pop().Move("selector").GetInnerText()->Uni(entities))
						{
							from_version += hstr->W;
							string_replace(from_version, _T('/'), _T('\\'));
							hstr->Free();
						}
						DIFFITEM *di = m_pCtxt->AddDiff(NULL);
						di->diffcode = DIFFCODE::NEEDSCAN;
						di->left.path = di->right.path = paths_GetParentPath(path.c_str());
						di->left.filename = di->right.filename = PathFindFileName(path.c_str());
						switch (idLeftContent)
						{
						case ID_MRGMAN_BASE:
							di->left.filename += base_version;
							break;
						case ID_MRGMAN_SOURCE:
							di->left.filename += from_version;
							break;
						}
						switch (idRightContent)
						{
						case ID_MRGMAN_BASE:
							di->right.filename += base_version;
							break;
						case ID_MRGMAN_SOURCE:
							di->right.filename += from_version;
							break;
						}
					}
					do; while (xml.Move());
					xml.Push();
				}
				do; while (xml.Move());
				xml.Push();
			}
			do; while (xml.Move());
			xml.Push();
		}
		do; while (xml.Move());
		xml.Push();
	}

	Redisplay();

	if (bNeedCompare)
		if (int nCompareSelected = m_pDirView->GetItemCount())
			Rescan(nCompareSelected);
}

/**
 * @brief Tell if user may use ".." and move to parents directory.
 * This function checks if current compare's parent folders should
 * be allowed to open. I.e. if current compare folders are:
 * - C:\Work\Project1 and
 * - C:\Work\Project2
 * we check if C:\Work and C:\Work should be allowed to opened.
 * For regular folders we allow opening if both folders exist.
 * @param [out] leftParent Left parent folder to open.
 * @param [out] rightParent Right parent folder to open.
 * @return Info if opening parent folders should be enabled:
 * - No : upward RESTRICTED
 * - ParentIsRegularPath : upward ENABLED
 * - ParentIsTempPath : upward ENABLED
 */
CDirFrame::AllowUpwardDirectory::ReturnCode
CDirFrame::AllowUpwardDirectory(String &leftParent, String &rightParent)
{
	const String &left = m_pCtxt->GetLeftPath();
	const String &right = m_pCtxt->GetRightPath();

	// If we have temp context it means we are comparing archives
	if (m_pTempPathContext != NULL)
	{
		if (m_nRecursive) // recursive mode (including tree-mode)
		{
			leftParent = m_pTempPathContext->m_strLeftParent;
			rightParent = m_pTempPathContext->m_strRightParent;
			return AllowUpwardDirectory::ParentIsTempPath;
		}

		LPCTSTR lname = PathFindFileName(left.c_str());
		LPCTSTR rname = PathFindFileName(right.c_str());
		String::size_type cchLeftRoot = m_pTempPathContext->m_strLeftRoot.length();

		if (left.length() <= cchLeftRoot + 1) // + 1 balances a trailing backslash
		{
			if (m_pTempPathContext->m_pParent)
			{
				leftParent = m_pTempPathContext->m_pParent->m_strLeftRoot;
				rightParent = m_pTempPathContext->m_pParent->m_strRightRoot;
				if (GetPairComparability(leftParent.c_str(), rightParent.c_str()) != IS_EXISTING_DIR)
					return AllowUpwardDirectory::Never;
				return AllowUpwardDirectory::ParentIsTempPath;
			}
			leftParent = m_pTempPathContext->m_strLeftDisplayRoot;
			rightParent = m_pTempPathContext->m_strRightDisplayRoot;
			if (!m_pCtxt->m_piFilterGlobal->includeFile(_T(""), leftParent.c_str(), _T(""), rightParent.c_str()))
				return AllowUpwardDirectory::Never;
			if (lstrcmpi(lname, _T("ORIGINAL\\")) == 0 && lstrcmpi(rname, _T("ALTERED\\")) == 0)
			{
				leftParent = paths_GetParentPath(leftParent.c_str());
				rightParent = paths_GetParentPath(rightParent.c_str());
			}
			lname = PathFindFileName(leftParent.c_str());
			rname = PathFindFileName(rightParent.c_str());
			if (lstrcmpi(lname, rname) == 0)
			{
				leftParent = paths_GetParentPath(leftParent.c_str());
				rightParent = paths_GetParentPath(rightParent.c_str());
				if (GetPairComparability(leftParent.c_str(), rightParent.c_str()) != IS_EXISTING_DIR)
					return AllowUpwardDirectory::Never;
				return AllowUpwardDirectory::ParentIsTempPath;
			}
			return AllowUpwardDirectory::No;
		}
	}
	// Don't attempt to go above root
	leftParent = paths_GetParentPath(left.c_str());
	rightParent = paths_GetParentPath(right.c_str());
	if (PathSkipRoot(leftParent.c_str()) && PathSkipRoot(rightParent.c_str()))
		return AllowUpwardDirectory::ParentIsRegularPath;
	return AllowUpwardDirectory::Never;
}

/**
 * @brief Refresh cached options.
 *
 * NB: This cancels any compare method specified on the command line.
 */
void CDirFrame::RefreshOptions()
{
	m_nCompMethod = -1;
}

/**
 * @brief Perform directory comparison again from scratch
 */
void CDirFrame::Rescan(int nCompareSelected)
{
	if (!m_pCtxt)
		return;

	// If we're already doing a rescan, bail out
	if (m_pCtxt->IsBusy())
		return;

	waitStatusCursor.Begin(IDS_STATUS_RESCANNING);
	UpdateCmdUI<ID_REFRESH>();

	LogFile.Write(CLogFile::LNOTICE, _T("Starting directory scan:\n\tLeft: %s\n\tRight: %s\n"),
			m_pCtxt->GetLeftPath().c_str(), m_pCtxt->GetRightPath().c_str());
	m_pCompareStats->Reset();
	m_pDirView->StartCompare();

	m_pCtxt->m_options.nIgnoreWhitespace = COptionsMgr::Get(OPT_CMP_IGNORE_WHITESPACE);
	m_pCtxt->m_options.bIgnoreBlankLines = COptionsMgr::Get(OPT_CMP_IGNORE_BLANKLINES);
	m_pCtxt->m_options.bFilterCommentsLines = COptionsMgr::Get(OPT_CMP_FILTER_COMMENTLINES);
	m_pCtxt->m_options.bIgnoreCase = COptionsMgr::Get(OPT_CMP_IGNORE_CASE);
	m_pCtxt->m_options.bIgnoreEol = COptionsMgr::Get(OPT_CMP_IGNORE_EOL);
	m_pCtxt->m_options.bApplyLineFilters = COptionsMgr::Get(OPT_LINEFILTER_ENABLED);
	m_pCtxt->m_nCompMethod = m_nCompMethod != -1 ? m_nCompMethod : COptionsMgr::Get(OPT_CMP_METHOD);
	m_pCtxt->m_bGuessEncoding = COptionsMgr::Get(OPT_CP_DETECT);
	m_pCtxt->m_bIgnoreSmallTimeDiff = COptionsMgr::Get(OPT_IGNORE_SMALL_FILETIME);
	m_pCtxt->m_bStopAfterFirstDiff = COptionsMgr::Get(OPT_CMP_STOP_AFTER_FIRST);
	m_pCtxt->m_nQuickCompareLimit = COptionsMgr::Get(OPT_CMP_QUICK_LIMIT);
	m_pCtxt->m_nBinaryCompareLimit = COptionsMgr::Get(OPT_CMP_BINARY_LIMIT);
	m_pCtxt->m_bSelfCompare = COptionsMgr::Get(OPT_CMP_SELF_COMPARE);
	m_pCtxt->m_bWalkUniques = COptionsMgr::Get(OPT_CMP_WALK_UNIQUES);

	// Set total items count since we don't collect items
	// Don't clear if only scanning selected items
	bool bDisableFilter = false;
	if (nCompareSelected)
	{
		m_pCompareStats->SetTotalItems(nCompareSelected);
		if (m_pMDIFrame->m_pCollectingDirFrame == this)
			bDisableFilter = true;
	}
	else
	{
		m_pDirView->DeleteAllItems();
		m_pCtxt->RemoveAll();
		if (m_pTempPathContext != NULL)
			bDisableFilter = true;
	}

	if (bDisableFilter)
	{
		m_pCtxt->m_piFilterGlobal = &transparentFileFilter;
		SetFilterStatusDisplay(NULL);
	}
	else
	{
		m_pCtxt->m_piFilterGlobal = &globalFileFilter;
		// Make sure filters are up-to-date
		globalFileFilter.ReloadCurrentFilter();
		// Show active filter name in statusbar
		SetFilterStatusDisplay(globalFileFilter.GetFilterNameOrMask().c_str());
	}

	// Folder names to compare are in the compare context
	m_pCtxt->CompareDirectories(nCompareSelected != 0);
}

/**
 * @brief Determines if the user wants to see given item.
 * This function determines what items to show and what items to hide. There
 * are lots of combinations, but basically we check if menuitem is enabled or
 * disabled and show/hide matching items. For non-recursive compare we never
 * hide folders as that would disable user browsing into them. And we even
 * don't really know if folders are identical or different as we haven't
 * compared them.
 * @param [in] di Item to check.
 * @return true if item should be shown, false if not.
 * @sa CDirFrame::Redisplay()
 */
bool CDirFrame::IsShowable(const DIFFITEM *di) const
{
	if (di->customFlags1 & ViewCustomFlags::HIDDEN)
		return false;
	// Treat SKIPPED as a 'super'-flag. If item is skipped and user
	// wants to see skipped items show item regardless of other flags
	if (di->isResultFiltered())
		return COptionsMgr::Get(OPT_SHOW_SKIPPED);
	// left/right filters
	if (di->isSideLeftOnly() && !COptionsMgr::Get(OPT_SHOW_UNIQUE_LEFT))
		return false;
	if (di->isSideRightOnly() && !COptionsMgr::Get(OPT_SHOW_UNIQUE_RIGHT))
		return false;
	if (di->isDirectory())
	{
		// Subfolders in non-recursive compare can only be skipped or unique
		if (m_nRecursive) // recursive mode (including tree-mode)
		{
			// ONLY filter folders by result (identical/different) for tree-view.
			// In the tree-view we show subfolders with identical/different
			// status. The flat view only shows files inside folders. So if we
			// filter by status the files inside folder are filtered too and
			// users see files appearing/disappearing without clear logic.
			if (COptionsMgr::Get(OPT_TREE_MODE))
			{
				// Need to stay visible due to presence of identical child items?
				if (di->containsIdentical() && COptionsMgr::Get(OPT_SHOW_IDENTICAL))
					return true;
				// Need to stay visible due to presence of unique child items?
				if (di->containsLeftOnly() && COptionsMgr::Get(OPT_SHOW_UNIQUE_LEFT))
					return true;
				if (di->containsRightOnly() && COptionsMgr::Get(OPT_SHOW_UNIQUE_RIGHT))
					return true;
				// result filters
				if (di->isResultSame() && !COptionsMgr::Get(OPT_SHOW_IDENTICAL))
					return false;
				if (di->isResultDiff() && !COptionsMgr::Get(OPT_SHOW_DIFFERENT))
					return false;
			}
		}
	}
	else
	{
		// file type filters
		if (di->isBin() && !COptionsMgr::Get(OPT_SHOW_BINARIES))
			return false;
		// result filters
		if (di->isResultSame() && !COptionsMgr::Get(OPT_SHOW_IDENTICAL))
			return false;
		if (di->isResultDiff() && !COptionsMgr::Get(OPT_SHOW_DIFFERENT))
			return false;
	}
	return true;
}

/**
 * @brief Empty & reload listview (of files & columns) with comparison results
 * @todo Better solution for special items ("..")?
 */
void CDirFrame::Redisplay()
{
	// Do not redisplay an empty CDirView
	// Not only does it not have results, but AddSpecialItems will crash
	// trying to dereference null context pointer to get to paths
	if (m_pCtxt)
		m_pDirView->Redisplay();
}

/**
 * @brief Update in-memory diffitem status from disk and update view.
 * @param [in] diffPos POSITION of item in UI list.
 * @param [in] bLeft If TRUE left-side item is updated.
 * @param [in] bRight If TRUE right-side item is updated.
 * @note Do not call this function from DirView code! This function
 * calls slow DirView functions to get item position and to update GUI.
 * Use UpdateStatusFromDisk() function instead.
 */
void CDirFrame::ReloadItemStatus(DIFFITEM *di, bool bLeft, bool bRight)
{
	// in case just copied (into existence) or modified
	m_pCtxt->UpdateStatusFromDisk(di, bLeft, bRight);
	int nIdx = m_pDirView->GetItemIndex(di);
	if (nIdx != -1)
	{
		// Update view
		m_pDirView->UpdateDiffItemStatus(nIdx);
	}
}

/**
 * @brief Find the CDiffContext diffpos of an item from its left & right paths
 * @return POSITION to item, NULL if not found.
 * @note Filenames must be same, if they differ NULL is returned.
 */
DIFFITEM *CDirFrame::FindItemFromPaths(LPCTSTR pathLeft, LPCTSTR pathRight)
{
	LPCTSTR file1 = PathFindFileName(pathLeft);
	LPCTSTR file2 = PathFindFileName(pathRight);

	String path1(pathLeft, static_cast<String::size_type>(file1 - pathLeft)); // include trailing backslash
	String path2(pathRight, static_cast<String::size_type>(file2 - pathRight)); // include trailing backslash

	String base1 = m_pCtxt->GetLeftPath();
	if (path1.compare(0, base1.length(), base1.c_str()) != 0)
		return NULL;
	path1.erase(0, base1.length()); // turn into relative path
	if (String::size_type length = path1.length())
		path1.resize(length - 1); // remove trailing backslash

	String base2 = m_pCtxt->GetRightPath();
	if (path2.compare(0, base2.length(), base2.c_str()) != 0)
		return NULL;
	path2.erase(0, base2.length()); // turn into relative path
	if (String::size_type length = path2.length())
		path2.resize(length - 1); // remove trailing backslash

	DIFFITEM *di = m_pCtxt->GetFirstChildDiff(NULL);
	while (di)
	{
		if (di->left.path == path1 &&
			di->right.path == path2 &&
			di->left.filename == file1 &&
			di->right.filename == file2)
		{
			break;
		}
		di = m_pCtxt->GetNextDiff(di);
	}
	return di;
}

/**
 * @brief A new MergeDoc has been opened.
 */
void CDirFrame::AddMergeDoc(CChildFrame *pDocFrame)
{
	m_MergeDocs.push_back(pDocFrame);
}

/**
 * @brief A new HexMergeDoc has been opened.
 */
void CDirFrame::AddMergeDoc(CHexMergeFrame *pDocFrame)
{
	m_HexMergeDocs.push_back(pDocFrame);
}

/**
 * @brief A new ImgMergeDoc has been opened.
 */
void CDirFrame::AddMergeDoc(CImgMergeFrame *pDocFrame)
{
	m_ImgMergeDocs.push_back(pDocFrame);
}

/**
 * @brief A new SQLiteMergeDoc has been opened.
 */
void CDirFrame::AddMergeDoc(CSQLiteMergeFrame *pDocFrame)
{
	m_SQLiteMergeDocs.push_back(pDocFrame);
}

/**
 * @brief A new ImgMergeDoc has been opened.
 */
void CDirFrame::AddMergeDoc(CReoGridMergeFrame *pDocFrame)
{
	m_ReoGridMergeDocs.push_back(pDocFrame);
}

/**
 * @brief MergeDoc informs us it is closing.
 */
void CDirFrame::MergeDocClosing(CChildFrame *pDocFrame)
{
	MergeDocPtrList::iterator pos = std::find(
		m_MergeDocs.begin(), m_MergeDocs.end(), pDocFrame);
	if (pos != m_MergeDocs.end())
		m_MergeDocs.erase(pos);
	else
		assert(false);
}

/**
 * @brief MergeDoc informs us it is closing.
 */
void CDirFrame::MergeDocClosing(CHexMergeFrame *pDocFrame)
{
	HexMergeDocPtrList::iterator pos = std::find(
		m_HexMergeDocs.begin(), m_HexMergeDocs.end(), pDocFrame);
	if (pos != m_HexMergeDocs.end())
		m_HexMergeDocs.erase(pos);
	else
		assert(false);
}

/**
 * @brief MergeDoc informs us it is closing.
 */
void CDirFrame::MergeDocClosing(CImgMergeFrame *pDocFrame)
{
	ImgMergeDocPtrList::iterator pos = std::find(
		m_ImgMergeDocs.begin(), m_ImgMergeDocs.end(), pDocFrame);
	if (pos != m_ImgMergeDocs.end())
		m_ImgMergeDocs.erase(pos);
	else
		assert(false);
}

/**
 * @brief MergeDoc informs us it is closing.
 */
void CDirFrame::MergeDocClosing(CSQLiteMergeFrame *pDocFrame)
{
	SQLiteMergeDocPtrList::iterator pos = std::find(
		m_SQLiteMergeDocs.begin(), m_SQLiteMergeDocs.end(), pDocFrame);
	if (pos != m_SQLiteMergeDocs.end())
		m_SQLiteMergeDocs.erase(pos);
	else
		assert(false);
}

/**
 * @brief MergeDoc informs us it is closing.
 */
void CDirFrame::MergeDocClosing(CReoGridMergeFrame *pDocFrame)
{
	ReoGridMergeDocPtrList::iterator pos = std::find(
		m_ReoGridMergeDocs.begin(), m_ReoGridMergeDocs.end(), pDocFrame);
	if (pos != m_ReoGridMergeDocs.end())
		m_ReoGridMergeDocs.erase(pos);
	else
		assert(false);
}

/**
 * @brief Close MergeDocs opened from DirDoc.
 *
 * Asks confirmation for docs containing unsaved data and then
 * closes MergeDocs.
 * @return TRUE if success, FALSE if user canceled or closing failed
 */
bool CDirFrame::CloseMergeDocs()
{
	while (!m_MergeDocs.empty())
	{
		CChildFrame *pDocFrame = m_MergeDocs.front();
		if (!pDocFrame->SaveModified())
			return false;
		pDocFrame->DestroyFrame();
	}
	while (!m_HexMergeDocs.empty())
	{
		CHexMergeFrame *pDocFrame = m_HexMergeDocs.front();
		if (!pDocFrame->SaveModified())
			return false;
		pDocFrame->DestroyFrame();
	}
	while (!m_ImgMergeDocs.empty())
	{
		CImgMergeFrame *pDocFrame = m_ImgMergeDocs.front();
		if (!pDocFrame->SaveModified())
			return false;
		pDocFrame->DestroyFrame();
	}
	while (!m_SQLiteMergeDocs.empty())
	{
		CSQLiteMergeFrame *pDocFrame = m_SQLiteMergeDocs.front();
		if (!pDocFrame->SaveModified())
			return false;
		pDocFrame->DestroyFrame();
	}
	while (!m_ReoGridMergeDocs.empty())
	{
		CReoGridMergeFrame *pDocFrame = m_ReoGridMergeDocs.front();
		if (!pDocFrame->SaveModified())
			return false;
		pDocFrame->DestroyFrame();
	}
	return true;
}

/**
 * @brief Obtain a merge doc to display a difference in files.
 * @return Pointer to CMergeDoc to use (new or existing).
 */
CChildFrame *CDirFrame::GetMergeDocForDiff()
{
	CChildFrame *pDocFrame = new CChildFrame(m_pMDIFrame, this);
	AddMergeDoc(pDocFrame);
	return pDocFrame;
}

/**
 * @brief Obtain a hex merge doc to display a difference in files.
 * @return Pointer to CHexMergeDoc to use (new or existing).
 */
CHexMergeFrame *CDirFrame::GetHexMergeDocForDiff()
{
	CHexMergeFrame *pDocFrame = new CHexMergeFrame(m_pMDIFrame, this);
	AddMergeDoc(pDocFrame);
	return pDocFrame;
}

/**
 * @brief Obtain an img merge doc to display a difference in files.
 * @return Pointer to CImgMergeDoc to use (new or existing).
 */
CImgMergeFrame *CDirFrame::GetImgMergeDocForDiff()
{
	CImgMergeFrame *pDocFrame = new CImgMergeFrame(m_pMDIFrame, this);
	AddMergeDoc(pDocFrame);
	return pDocFrame;
}

/**
 * @brief Obtain an SQLite merge doc to display a difference in files.
 * @return Pointer to CSQLiteMergeDoc to use (new or existing).
 */
CSQLiteMergeFrame *CDirFrame::GetSQLiteMergeDocForDiff()
{
	CSQLiteMergeFrame *pDocFrame = new CSQLiteMergeFrame(m_pMDIFrame, this);
	AddMergeDoc(pDocFrame);
	return pDocFrame;
}

/**
 * @brief Obtain an ReoGrid merge doc to display a difference in files.
 * @return Pointer to CReoGridMergeDoc to use (new or existing).
 */
CReoGridMergeFrame *CDirFrame::GetReoGridMergeDocForDiff()
{
	CReoGridMergeFrame *pDocFrame = new CReoGridMergeFrame(m_pMDIFrame, this);
	AddMergeDoc(pDocFrame);
	return pDocFrame;
}

/**
 * @brief Update changed item's compare status
 * @param [in] paths Paths for files we update
 * @param [in] nDiffs Total amount of differences
 * @param [in] nTrivialDiffs Amount of ignored differences
 * @param [in] bIdentical TRUE if files became identical, FALSE otherwise.
 */
void CDirFrame::UpdateChangedItem(const CChildFrame *pMergeDoc)
{
	LPCTSTR paths[2] =
	{
		pMergeDoc->m_strPath[0].c_str(),
		pMergeDoc->m_strPath[1].c_str()
	};

	DIFFITEM *di = FindItemFromPaths(paths[0], paths[1]);
	// If we failed files could have been swapped so lets try again
	if (!di)
		di = FindItemFromPaths(paths[1], paths[0]);

	// Update status if paths were found for items.
	// Fail means we had unique items compared as 'renamed' items
	// so there really is not status to update.
	if (di)
	{
		ReloadItemStatus(di, TRUE, TRUE);

		di->nsdiffs = pMergeDoc->m_diffList.GetSignificantDiffs();
		di->nidiffs = pMergeDoc->m_nTrivialDiffs;

		di->diffcode &= ~(DIFFCODE::COMPAREFLAGS | DIFFCODE::TEXTFLAGS);
		di->diffcode |= di->nsdiffs ? DIFFCODE::DIFF : DIFFCODE::SAME;

		di->left.m_textStats = pMergeDoc->m_pFileTextStats[0];
		di->right.m_textStats = pMergeDoc->m_pFileTextStats[1];

		if (di->left.m_textStats.nzeros > 0)
			di->diffcode |= DIFFCODE::BINSIDE1 | DIFFCODE::TEXT;
		if (di->right.m_textStats.nzeros > 0)
			di->diffcode |= DIFFCODE::BINSIDE2 | DIFFCODE::TEXT;
		di->diffcode ^= DIFFCODE::TEXT; // revert reverse logic
	}
}

/**
 * @brief Cleans up after directory compare
 */
void CDirFrame::CompareReady()
{
	LogFile.Write(CLogFile::LNOTICE, _T("Directory scan complete\n"));
	waitStatusCursor.End();
	UpdateCmdUI<ID_REFRESH>();
}

/**
 * @brief Write path and filename to headerbar
 * @note SetText() does not repaint unchanged text
 */
void CDirFrame::UpdateHeaderPath(BOOL bLeft)
{
	int nPane = 0;
	String sText;

	if (bLeft)
	{
		if (!m_strLeftDesc.empty())
			sText = m_strLeftDesc;
		else
		{
			sText = m_pCtxt->GetLeftPath();
			ApplyDisplayRoot(sText);
		}
		nPane = 0;
	}
	else
	{
		if (!m_strRightDesc.empty())
			sText = m_strRightDesc;
		else
		{
			sText = m_pCtxt->GetRightPath();
			ApplyDisplayRoot(sText);
		}
		nPane = 1;
	}

	m_wndFilePathBar.SetText(nPane, sText.c_str(), FALSE);
}

/**
 * @brief Send signal to thread to stop current scan
 *
 * @sa CDirCompStateBar::OnStop()
 */
void CDirFrame::AbortCurrentScan()
{
	LogFile.Write(CLogFile::LNOTICE, _T("Dircompare aborted!"));
	m_pCtxt->Abort();
}

/**
 * @brief Set directory description texts shown in headerbar
 */
void CDirFrame::SetDescriptions(const String &strLeftDesc, const String &strRightDesc)
{
	m_strLeftDesc = strLeftDesc;
	m_strRightDesc = strRightDesc;
	UpdateTitle();
}

void CDirFrame::SwapSides()
{
	WaitStatusCursor waitCursor;
	eastl::swap(m_bROLeft, m_bRORight);
	eastl::swap(m_strLeftDesc, m_strRightDesc);
	// Loop through the cached contexts
	LIST_ENTRY *entry = &m_root;
	while ((entry = entry->Flink) != &m_root)
	{
		CDiffContext *pCtxt = static_cast<CDiffContext *>(entry);
		pCtxt->SwapSides();
		// Loop through the cached items
		DIFFITEM *di = NULL;
		while ((di = pCtxt->GetNextDiff(di)) != NULL)
		{
			eastl::swap(di->left, di->right);
			switch (di->diffcode & DIFFCODE::SIDEFLAGS)
			{
			case DIFFCODE::LEFT:
			case DIFFCODE::RIGHT:
				di->diffcode ^= DIFFCODE::LEFT ^ DIFFCODE::RIGHT;
				break;
			}
		}
	}
	m_pCompareStats->SwapSides();
	if (m_pTempPathContext)
		m_pTempPathContext->SwapSides();
	// Update UI
	UpdateTitle();
	Redisplay();
}

void CDirFrame::UpdateTitle()
{
	String strTitle;
	if (!m_pCtxt || m_pCtxt->GetLeftPath().empty() ||
		m_pCtxt->GetRightPath().empty())
	{
		strTitle = LanguageSelect.LoadString(IDS_DIRECTORY_WINDOW_TITLE);
	}
	else
	{
		const TCHAR strSeparator[] = _T(" - ");
		String strPath = m_pCtxt->GetLeftPath();
		ApplyDisplayRoot(strPath);
		strTitle = PathFindFileName(strPath.c_str());
		strTitle += strSeparator;
		strPath = m_pCtxt->GetRightPath();
		ApplyDisplayRoot(strPath);
		strTitle += PathFindFileName(strPath.c_str());
	}
	SetWindowText(strTitle.c_str());
	UpdateHeaderPath(0);
	UpdateHeaderPath(1);
}

/**
 * @brief Replace internal root by display root.
 * When we have an archive file open, this function converts physical folder
 * (that is in the temp folder where archive was extracted) to the virtual
 * path for showing. The virtual path is path to the archive file, archive
 * file name and folder inside the archive.
 * @param [in, out] sText Path to convert.
 */
void CDirFrame::ApplyDisplayRoot(String &sText)
{
	if (m_pTempPathContext)
		m_pTempPathContext->ApplyDisplayRoot(sText);
}

/**
 * @brief Update results for FileActionItem.
 * This functions is called to update DIFFITEM after FileActionItem.
 * @param [in] act Action that was done.
 * @param [in] bMakeTargetItemWritable Whether to make target item writable.
 * @return Whether any detachments occurred.
 */
bool CDirFrame::UpdateDiffAfterOperation(const FileActionItem &act, bool bMakeTargetItemWritable)
{
	DIFFITEM *di = m_pDirView->GetDiffItem(act.context);
	if (di == NULL)
		return false;
	bool bUpdateLeft = false;
	bool bUpdateRight = false;
	bool bDetachments = false;
	bool bPreserveInfo = false;
	// Use FileActionItem types for simplicity for now.
	// Better would be to use FileAction contained, since it is not
	// UI dependent.
	switch (act.UIResult)
	{
	case FileActionItem::UI_SYNC:
		// Synchronized items may need VCS operations
		if (m_pMDIFrame->m_bCheckinVCS && di->isSideBoth())
		{
			CMainFrame::CreateCaret(static_cast<HListView *>(m_pDirView->m_pWnd), act.context);
			m_pMDIFrame->CheckinToClearCase(act.dest.c_str());
			DestroyCaret();
		}
		di->diffcode &= ~(DIFFCODE::SIDEFLAGS | DIFFCODE::COMPAREFLAGS);
		if (act.dirflag)
			di->diffcode |= DIFFCODE::BOTH | DIFFCODE::NOCMP;
		else
			di->diffcode |= DIFFCODE::BOTH | DIFFCODE::SAME;
		di->nidiffs = 0;
		di->nsdiffs = 0;
		switch (act.UIDestination)
		{
		case FileActionItem::UI_LEFT:
			bUpdateLeft = true;
			bPreserveInfo = true;
			di->left.versionChecked = di->right.versionChecked;
			di->left.version = di->right.version;
			di->left.encoding = di->right.encoding;
			di->left.m_textStats = di->right.m_textStats;
			break;

		case FileActionItem::UI_RIGHT:
			bUpdateRight = true;
			bPreserveInfo = true;
			di->right.versionChecked = di->left.versionChecked;
			di->right.version = di->left.version;
			di->right.encoding = di->left.encoding;
			di->right.m_textStats = di->left.m_textStats;
			break;
		}
		break;

	case FileActionItem::UI_DEL_LEFT:
		if (di->isSideLeftOnly())
		{
			m_pDirView->DetachItem(act.context);
			m_pCtxt->RemoveDiff(di);
			bDetachments = true;
		}
		else
		{
			di->diffcode &= ~(DIFFCODE::SIDEFLAGS | DIFFCODE::COMPAREFLAGS);
			di->diffcode |= DIFFCODE::RIGHT | DIFFCODE::NOCMP;
			bUpdateLeft = true;
		}
		break;

	case FileActionItem::UI_DEL_RIGHT:
		if (di->isSideRightOnly())
		{
			m_pDirView->DetachItem(act.context);
			m_pCtxt->RemoveDiff(di);
			bDetachments = true;
		}
		else
		{
			di->diffcode &= ~(DIFFCODE::SIDEFLAGS | DIFFCODE::COMPAREFLAGS);
			di->diffcode |= DIFFCODE::LEFT | DIFFCODE::NOCMP;
			bUpdateRight = true;
		}
		break;

	case FileActionItem::UI_DEL_BOTH:
		m_pDirView->DetachItem(act.context);
		m_pCtxt->RemoveDiff(di);
		bDetachments = true;
		break;
	}
	if (bUpdateLeft || bUpdateRight)
	{
		m_pCtxt->UpdateStatusFromDisk(di, bUpdateLeft, bUpdateRight, bMakeTargetItemWritable, bPreserveInfo);
		m_pDirView->UpdateDiffItemStatus(act.context);
	}
	return bDetachments;
}

/**
 * @brief Set item's view-flag.
 * @param [in] key Item fow which flag is set.
 * @param [in] flag Flag value to set.
 * @param [in] mask Mask for possible flag values.
 */
void CDirFrame::SetItemViewFlag(DIFFITEM *di, UINT flag, UINT mask)
{
	di->customFlags1 &= ~mask; // Zero bits masked
	di->customFlags1 |= flag;
}

/**
 * @brief Set all item's view-flag.
 * @param [in] flag Flag value to set.
 * @param [in] mask Mask for possible flag values.
 */
void CDirFrame::SetItemViewFlag(UINT flag, UINT mask)
{
	DIFFITEM *di = m_pCtxt->GetFirstChildDiff(NULL);
	while (di != NULL)
	{
		di->customFlags1 &= ~mask; // Zero bits masked
		di->customFlags1 |= flag;
		di = m_pCtxt->GetNextDiff(di);
	}
}
