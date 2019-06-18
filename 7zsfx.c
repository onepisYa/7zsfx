#include <Windows.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <stdbool.h>
#include <windowsx.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib,"shlwapi.lib")

int CALLBACK ChooseDirectoryClassicCbk(HWND hwnd, UINT msg, LPARAM lParam, LPARAM lpData)
{
	switch (msg)
	{
	case BFFM_INITIALIZED:
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		break;
	case BFFM_SELCHANGED:
		SendMessage(hwnd, BFFM_SETSELECTION, FALSE, lParam);
		break;
	}
	return 0;
}

BOOL ChooseDirectoryClassic(HWND hWndParent, TCHAR* fullPath, PCTSTR pcszDefaultPath, PCTSTR pcszInstruction)
{
	BROWSEINFO bi;
	bi.hwndOwner = hWndParent;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = fullPath;
	bi.lpszTitle = pcszInstruction;
	bi.ulFlags = BIF_USENEWUI | BIF_UAHINT;
	bi.lParam = (LPARAM)pcszDefaultPath;
	//https://www.arclab.com/en/kb/cppmfc/select-folder-shbrowseforfolder.html
	bi.lpfn = ChooseDirectoryClassicCbk;
	bi.iImage = 0;
	LPITEMIDLIST pi = SHBrowseForFolder(&bi);

	if (pi)
	{
		if (SHGetPathFromIDList(pi, fullPath))
			return TRUE;
	}
	return FALSE;
}

void OnInitDialog(HWND hwnd)
{
	CheckDlgButton(hwnd, IDC_CHECK_PROGRESS, BST_CHECKED);
	CheckDlgButton(hwnd, IDC_CHECK_LOCATE, BST_CHECKED);
	if (__argc > 1)
		SetDlgItemText(hwnd, IDC_EDIT_SOURCE, __wargv[1]);
}

BOOL IsExecutableFile(LPCTSTR filename)
{
	LPCTSTR ext[] = { TEXT(".exe"),TEXT(".com") };
	for (int i = 0; i < ARRAYSIZE(ext); i++)
	{
		if (StrCmpI(StrRChr(filename, NULL, '.'), ext[i]) == 0)
			return TRUE;
	}
	return FALSE;
}

BOOL IsScriptFile(LPCTSTR filename)
{
	LPCTSTR ext[] = { TEXT(".bat"),TEXT(".cmd"),TEXT(".js"),TEXT(".vbs") };
	for (int i = 0; i < ARRAYSIZE(ext); i++)
	{
		if (StrCmpI(StrRChr(filename, NULL, '.'), ext[i]) == 0)
			return TRUE;
	}
	return FALSE;
}

void ShowBalloonTip(HWND hwnd, int id, LPCTSTR msg,LPCTSTR title)
{
	EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP), title,msg,TTI_ERROR_LARGE };
	Edit_ShowBalloonTip(GetDlgItem(hwnd, id), &tip);
	MessageBeep(0);
}

BOOL IsDirectoryEmpty(LPCTSTR path)
{
	WIN32_FIND_DATA fd;
	TCHAR checkpath[MAX_PATH] = TEXT("");
	PathCombine(checkpath, path, TEXT("*"));
	HANDLE fh = FindFirstFile(checkpath, &fd);
	if (fh == INVALID_HANDLE_VALUE)
		return TRUE;
	int c = 0;
	for (BOOL success = TRUE; success; success = FindNextFile(fh, &fd))
		if (StrCmp(fd.cFileName, TEXT(".")) != 0 && StrCmp(fd.cFileName, TEXT("..")) != 0)
			c++;
	return c == 0;
}

int WCharToUtf8(LPCWSTR src,int slen,char *dst,int dlen)
{
	if (dlen < WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL))
		return 0;
	return WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dlen, NULL, NULL);
}

BOOL WriteProperty(FILE* fp, LPCSTR key, LPCTSTR val)
{
	char buf[400];
	int bc = WCharToUtf8(val, lstrlen(val), buf, 400);
	fprintf(fp, "%s=\"", key);
	if (fwrite(buf, 1, bc - 1, fp) == 0)
		return FALSE;
	fprintf(fp, "\"\n");
	return TRUE;
}

void OnOK(HWND hwnd)
{
	TCHAR buf[MAX_PATH] = TEXT("");
	GetDlgItemText(hwnd, IDC_EDIT_SOURCE, buf, MAX_PATH - 1);
	if (lstrlen(buf) == 0)
	{
		ShowBalloonTip(hwnd, IDC_EDIT_SOURCE, TEXT("������·����"),TEXT("����"));
		return;
	}
	else if(IsDirectoryEmpty(buf))
	{
		ShowBalloonTip(hwnd, IDC_EDIT_SOURCE, TEXT("����һ����Ч·������ļ��У���ѡ������·����"), TEXT("����"));
		return;
	}
	TCHAR fullpath[MAX_PATH] = TEXT(""), filename[MAX_PATH] = TEXT(""),pathname[MAX_PATH],drivename[16],extname[MAX_PATH];
	GetFullPathName(buf, MAX_PATH - 1, fullpath, NULL);
	_wsplitpath_s(fullpath, drivename, 16,pathname,MAX_PATH, filename, MAX_PATH, extname, MAX_PATH);
	PathCombine(fullpath, fullpath, TEXT("*"));

	FILE* fp = NULL;
	_wfopen_s(&fp, TEXT("config.txt"), TEXT("wb+"));
	if (fp)
	{
		fprintf(fp, ";!@Install@!UTF-8!\n");
		if (GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_TITLE)))
		{
			GetDlgItemText(hwnd, IDC_EDIT_TITLE, buf, MAX_PATH - 1);
			WriteProperty(fp, "Title", buf);
		}
		if (GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_MESSAGE)))
		{
			GetDlgItemText(hwnd, IDC_EDIT_MESSAGE, buf, MAX_PATH - 1);
			WriteProperty(fp, "BeginPrompt", buf);
		}
		WriteProperty(fp, "Progress", IsDlgButtonChecked(hwnd, IDC_CHECK_PROGRESS) ? TEXT("yes") : TEXT("no"));
		ComboBox_GetText(GetDlgItem(hwnd, IDC_COMBO_RUN), buf, MAX_PATH - 1);
		if (lstrlen(buf))
		{
			if (IsExecutableFile(buf))
			{
				WriteProperty(fp, "RunProgram", buf);
				if (GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_RUN_PATH)))
				{
					GetDlgItemText(hwnd, IDC_EDIT_RUN_PATH, buf, MAX_PATH - 1);
					WriteProperty(fp, "Directory", buf);
				}
			}
			else
			{
				WriteProperty(fp, "ExecuteFile", buf);
				if (GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_RUN_PATH)))
				{
					GetDlgItemText(hwnd, IDC_EDIT_RUN_PATH, buf, MAX_PATH - 1);
					WriteProperty(fp, "ExecuteParameters", buf);
				}
			}
		}
		fprintf(fp, ";!@InstallEnd@!\n");
		fclose(fp);
	}

	TCHAR cmdline[400];
	wsprintf(cmdline, TEXT("7za a archive.7z \"%s\"&copy/b/y 7zS.sfx+config.txt+archive.7z \"%s.exe\"&del config.txt archive.7z"), fullpath, filename);
	if (IsDlgButtonChecked(hwnd, IDC_CHECK_LOCATE))
	{
		lstrcat(cmdline, TEXT("&explorer/select,"));
		lstrcat(cmdline, filename);
		lstrcat(cmdline, TEXT(".exe"));
	}

	EndDialog(hwnd, 0);
	_wsystem(cmdline);
}

void RefreshCombobox(HWND hwnd,LPCTSTR path)
{
	HWND hCombobox = GetDlgItem(hwnd, IDC_COMBO_RUN);
	while (ComboBox_GetCount(hCombobox))
		ComboBox_DeleteString(hCombobox, 0);

	WIN32_FIND_DATA fd;
	TCHAR checkpath[MAX_PATH] = TEXT("");
	PathCombine(checkpath, path, TEXT("*"));
	HANDLE fh = FindFirstFile(checkpath, &fd);
	if (fh == INVALID_HANDLE_VALUE)
		return;
	for (BOOL success = TRUE; success; success = FindNextFile(fh, &fd))
	{
		if (IsExecutableFile(fd.cFileName)||IsScriptFile(fd.cFileName))
		{
			ComboBox_AddString(hCombobox, fd.cFileName);
			if (ComboBox_GetCount(hCombobox) == 1)
				ComboBox_SetCurSel(hCombobox, 0);
		}
	}
}

void OnBrowse(HWND hwnd)
{
	TCHAR path[MAX_PATH] = TEXT("");
	GetDlgItemText(hwnd, IDC_EDIT_SOURCE, path, MAX_PATH - 1);
	if (lstrlen(path) == 0)
		GetCurrentDirectory(MAX_PATH, path);
	if (ChooseDirectoryClassic(hwnd, path, path, TEXT("ѡ��Ҫѹ�����ļ�Ŀ¼��")))
		SetDlgItemText(hwnd, IDC_EDIT_SOURCE, path);
}

void OnEditSourceChange(HWND hwnd)
{
	TCHAR text[MAX_PATH] = TEXT("");
	GetDlgItemText(hwnd, IDC_EDIT_SOURCE, text, MAX_PATH - 1);
	RefreshCombobox(hwnd, text);
}

INT_PTR CALLBACK DialogFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		OnInitDialog(hwnd);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
			EndDialog(hwnd, 1);
			break;
		case IDOK:
			OnOK(hwnd);
			break;
		case IDC_BUTTON_BROWSE:
			OnBrowse(hwnd);
			break;
		case IDC_EDIT_SOURCE:
			if (HIWORD(wParam) == EN_CHANGE)
				OnEditSourceChange(hwnd);
			break;
		}
		break;
	case WM_DROPFILES:
	{
		TCHAR filepath[MAX_PATH] = TEXT("");
		DragQueryFile((HDROP)wParam, 0, filepath, MAX_PATH);
		DragFinish((HDROP)wParam);
		SetDlgItemText(hwnd, IDC_EDIT_SOURCE, filepath);
		break;
	}
	}
	return 0;
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE hPrevI, LPWSTR param, int iShowWindow)
{
	return (int)DialogBox(hI, MAKEINTRESOURCE(IDD_DIALOG_MAIN), NULL, DialogFunc);
}