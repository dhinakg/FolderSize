#pragma once

class ATL_NO_VTABLE CFolderSizeObj : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CFolderSizeObj, &CLSID_FolderSizeObj>,
	public IColumnProvider
{
public:
	enum FolderSizeColumns {
		FSC_SIZE = 0,
		FSC_FILES,
		FSC_FOLDERS,
		FSC_SIBLINGS
	};

	CFolderSizeObj()
	{
	}

	static void WINAPI ObjectMain(bool bStarting);

DECLARE_NO_REGISTRY()
	
DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CFolderSizeObj)
	COM_INTERFACE_ENTRY_IID(IID_IColumnProvider, IColumnProvider)
END_COM_MAP()

public:
// IFolderSizeObj
	STDMETHOD (Initialize)(LPCSHCOLUMNINIT psci);
	STDMETHOD (GetColumnInfo)(DWORD dwIndex, SHCOLUMNINFO *psci);
	STDMETHOD (GetItemData)(LPCSHCOLUMNID pscid, LPCSHCOLUMNDATA pscd, VARIANT *pvarData);
};

OBJECT_ENTRY_AUTO(CLSID_FolderSizeObj, CFolderSizeObj)
