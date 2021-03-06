/**
 * @file  OptionsPanel.cpp
 *
 * @brief Implementation of OptionsPanel class.
 */
#include "StdAfx.h"
#include "OptionsPanel.h"
#include "LanguageSelect.h"

/**
 * @brief Constructor.
 */
OptionsPanel::OptionsPanel(UINT nIDTemplate, size_t cb)
: ZeroInit<OptionsPanel>(cb)
, ODialog(nIDTemplate)
, m_nPageIndex(0)
{
}

BOOL OptionsPanel::OnInitDialog()
{
	ODialog::OnInitDialog();
	LanguageSelect.TranslateDialog(m_hWnd);
	UpdateScreen();
	return TRUE;
}

/**
 * @brief Let user browse common color dialog, and select a color
 */
void OptionsPanel::BrowseColor(int id, COLORREF &currentColor)
{
	if (SyntaxColors.ChooseColor(m_pWnd, id))
		currentColor = GetDlgItemInt(id);
}

void OptionsPanel::SerializeColor(OPERATION op, int id, COptionDef<COLORREF> &optionName, COLORREF &color)
{
	switch (op)
	{
	case SET_DEFAULTS:
		color = COptionsMgr::GetDefault(optionName);
		break;

	case WRITE_OPTIONS:
		COptionsMgr::SaveOption(optionName, color);
		break;

	case READ_OPTIONS:
		color = COptionsMgr::Get(optionName);
		break;

	case INVALIDATE:
		SetDlgItemInt(id, color);
		break;
	}
}

int OptionsPanel::ValidateNumber(HEdit *edit, int iMin, int iMax)
{
	const int nIDDlgItem = edit->GetDlgCtrlID();
	const BOOL bSigned = iMin < 0;
	BOOL bTranslated = FALSE;
	const int i = GetDlgItemInt(nIDDlgItem, &bTranslated, bSigned);
	if (bTranslated && i == 0)
	{
		if (edit->GetWindowTextLength() > 1)
			edit->SetWindowText(_T("0"));
		edit->SetSel(0, 1);
	}
	const int j = i < iMin ? iMin : i > iMax ? iMax : i;
	if (j != i)
	{
		if (edit->GetModify())
			MessageBeep(0);
		int sel = edit->GetSel();
		SetDlgItemInt(nIDDlgItem, j, bSigned);
		int len = edit->GetWindowTextLength();
		if (sel != 0)
			sel = len;
		edit->SetSel(sel, len);
	}
	return j;
}

void OptionsPanel::WildcardRemoveDuplicatePatterns(String &patterns)
{
	String::size_type i = 0, j = 0, k = 0;
	while ((j = patterns.find_first_of(_T("; "), i)) != String::npos &&
		(k = patterns.find_last_of(_T("; "), j) + 1) != patterns.length())
	{
		TCHAR const sep = patterns[j];
		patterns[j] = _T('\0');
		if (PathMatchSpec(patterns.c_str() + i, patterns.c_str() + k))
		{
			patterns.erase(i, k - i);
		}
		else
		{
			patterns[j] = sep;
			i = k;
		}
	}
}
