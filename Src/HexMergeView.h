/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
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
 * @file  HexMergeDoc.h
 *
 * @brief Declaration of CHexMergeDoc class
 */
class IMergeEditStatus;
class CHexMergeFrame;
class IHexEditorWindow;

/**
 * @brief Document class for bytewise merging two files presented as hexdumps
 */
class CHexMergeView : public OWindow
{
// Attributes
protected:
	IHexEditorWindow *m_pif;
public:
	int m_nThisPane;
	CHexMergeFrame *const m_pDocument;
	UINT64 m_mtime;
	DWORD m_size;
	HStatusBar *m_pStatusBar;
public:
	CHexMergeView(CHexMergeFrame *, int);
	static LPCTSTR RegisterClass();
	void Subclass(HWindow *pWnd)
	{
		OWindow::Subclass<0>(pWnd);
		m_pif = reinterpret_cast<IHexEditorWindow *>(::GetWindowLongPtr(pWnd->m_hWnd, GWLP_USERDATA));
		m_style = GetStyle();
	}
	BOOL PreTranslateMessage(MSG *);
	HRESULT LoadFile(LPCTSTR);
	HRESULT SaveFile(LPCTSTR);
	IHexEditorWindow *GetInterface() const { return m_pif; }
	IHexEditorWindow::Status *GetStatus();
	BYTE *GetBuffer(int);
	int GetLength();
	BOOL GetModified();
	void SetSavepoint();
	BOOL GetReadOnly();
	void SetReadOnly(BOOL);
	void ResizeWindow();
	void RepaintRange(int, int);
    void UpdateBookmarkMenu();
	BOOL IsFileChangedOnDisk(LPCTSTR);
	void ZoomText(int amount);
	// Overrides
protected:
	virtual LRESULT WindowProc(UINT, WPARAM, LPARAM);

// Implementation data
	LONG m_style;

// Message handlers
    void OnEraseBkgnd();
	void OnSize();

public:
	void OnEditFind();
	void OnEditReplace();
	void OnEditRepeat();
	void OnEditCut();
	void OnEditCopy();
	void OnEditPaste();
	void OnEditClear();
	void OnEditSelectAll();
	void OnEditUndo();
	void OnEditRedo();
	void OnEditGoto();
    void OnAddBookmark();
    void OnRemoveBookmark();
    void OnClearAllBookmark();
    void OnGotoBookmark(int);
	void OnFirstdiff();
	void OnLastdiff();
	void OnNextdiff();
	void OnPrevdiff();
};
