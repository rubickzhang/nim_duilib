#include "ListCtrlDataProvider.h"
#include "duilib/Control/ListCtrl.h"
#include "duilib/Core/GlobalManager.h"
#include <unordered_map>

namespace ui
{
ListCtrlDataProvider::ListCtrlDataProvider() :
    m_pListCtrl(nullptr),
    m_pfnCompareFunc(nullptr),
    m_pUserData(nullptr),
    m_hideRowCount(0),
    m_heightRowCount(0),
    m_atTopRowCount(0)
{
}

Control* ListCtrlDataProvider::CreateElement()
{
    ASSERT(m_pListCtrl != nullptr);
    if (m_pListCtrl == nullptr) {
        return nullptr;
    }
    ListCtrlItem* pItem = new ListCtrlItem;
    pItem->SetClass(m_pListCtrl->GetDataItemClass());
    return pItem;
}

bool ListCtrlDataProvider::FillElement(ui::Control* pControl, size_t nElementIndex)
{
    ASSERT(m_pListCtrl != nullptr);
    if (m_pListCtrl == nullptr) {
        return false;
    }
    ListCtrlHeader* pHeaderCtrl = m_pListCtrl->GetListCtrlHeader();
    ASSERT(pHeaderCtrl != nullptr);
    if (pHeaderCtrl == nullptr) {
        return false;
    }
    ListCtrlItem* pItem = dynamic_cast<ListCtrlItem*>(pControl);
    ASSERT(pItem != nullptr);
    if (pItem == nullptr) {
        return false;
    }
    // 基本结构: <ListCtrlItem> <LabelBox/> ... <LabelBox/>  </ListCtrlItem>
    // 附加说明: 1. ListCtrlItem 是 HBox的子类;   
    //          2. 每一列，放置一个LabelBox控件

    //获取需要显示的各个列的属性
    struct ElementData
    {
        size_t nColumnId = Box::InvalidIndex;
        int32_t nColumnWidth = 0;
        StoragePtr pStorage;
    };
    std::vector<ElementData> elementDataList;
    const size_t nColumnCount = pHeaderCtrl->GetColumnCount();
    for (size_t nColumnIndex = 0; nColumnIndex < nColumnCount; ++nColumnIndex) {
        ListCtrlHeaderItem* pHeaderItem = pHeaderCtrl->GetColumn(nColumnIndex);
        if ((pHeaderItem == nullptr) || !pHeaderItem->IsColumnVisible()){
            continue;
        }
        int32_t nColumnWidth = pHeaderCtrl->GetColumnWidth(nColumnIndex);
        if (nColumnWidth < 0) {
            nColumnWidth = 0;
        }
        ElementData data;
        data.nColumnId = pHeaderCtrl->GetColumnId(nColumnIndex);
        data.nColumnWidth = nColumnWidth;
        elementDataList.push_back(data);
    }
    std::vector<size_t> columnIdList;
    for (auto data : elementDataList) {
        columnIdList.push_back(data.nColumnId);
    }
    StoragePtrList storageList;
    if (!GetDataItemStorageList(nElementIndex, columnIdList, storageList)) {
        return false;
    }
    ASSERT(storageList.size() == elementDataList.size());
    for (size_t index = 0; index < storageList.size(); ++index) {
        elementDataList[index].pStorage = storageList[index];
    }
    storageList.clear();
    ASSERT(!elementDataList.empty());
    if (elementDataList.empty()) {
        return false;
    }

    const size_t showColumnCount = elementDataList.size(); //显示的列数
    while (pItem->GetItemCount() > showColumnCount) {
        //移除多余的列
        if (!pItem->RemoveItemAt(pItem->GetItemCount() - 1)) {
            ASSERT(!"RemoveItemAt failed!");
            return false;
        }
    }

    //默认属性
    std::wstring defaultLabelBoxClass = m_pListCtrl->GetDataItemLabelClass();
    LabelBox defaultLabelBox;
    if (!defaultLabelBoxClass.empty()) {
        defaultLabelBox.SetClass(defaultLabelBoxClass);
    }
    bool bFirstLine = (m_pListCtrl->GetTopDataItem() == nElementIndex);//是否为第一个数据行    
    for (size_t nColumn = 0; nColumn < showColumnCount; ++nColumn) {
        const ElementData& elementData = elementDataList[nColumn];
        LabelBox* pLabelBox = nullptr;
        if (nColumn < pItem->GetItemCount()) {
            pLabelBox = dynamic_cast<LabelBox*>(pItem->GetItemAt(nColumn));
            ASSERT(pLabelBox != nullptr);
            if (pLabelBox == nullptr) {
                return false;
            }
        }
        else {
            pLabelBox = new LabelBox;
            pItem->AddItem(pLabelBox);            
            if (!defaultLabelBoxClass.empty()) {
                pLabelBox->SetClass(defaultLabelBoxClass);
            }
            pLabelBox->SetMouseEnabled(false);
        }

        //填充数据，设置属性        
        pLabelBox->SetFixedWidth(UiFixedInt(elementData.nColumnWidth), true, false);
        const StoragePtr& pStorage = elementData.pStorage;
        if (pStorage != nullptr) {
            pLabelBox->SetText(pStorage->text.c_str());
            if (pStorage->nTextFormat != 0) {
                pLabelBox->SetTextStyle(pStorage->nTextFormat, false);
            }
            else {
                pLabelBox->SetTextStyle(defaultLabelBox.GetTextStyle(), false);
            }
            pLabelBox->SetTextPadding(defaultLabelBox.GetTextPadding(), false);
            if (!pStorage->textColor.IsEmpty()) {
                pLabelBox->SetStateTextColor(kControlStateNormal, pLabelBox->GetColorString(pStorage->textColor));
            }
            else {
                pLabelBox->SetStateTextColor(kControlStateNormal, defaultLabelBox.GetStateTextColor(kControlStateNormal));
            }
            if (!pStorage->bkColor.IsEmpty()) {
                pLabelBox->SetBkColor(pStorage->bkColor);
            }
            else {
                pLabelBox->SetBkColor(defaultLabelBox.GetBkColor());
            }
            if (pStorage->bShowCheckBox) {
                //添加CheckBox
                CheckBox* pCheckBox = nullptr;
                if (pLabelBox->GetItemCount() > 0) {
                    pCheckBox = dynamic_cast<CheckBox*>(pLabelBox->GetItemAt(0));
                }
                if (pCheckBox == nullptr) {
                    pCheckBox = new CheckBox;
                    std::wstring checkBoxClass = m_pListCtrl->GetCheckBoxClass();
                    ASSERT(!checkBoxClass.empty());
                    pLabelBox->AddItem(pCheckBox);
                    if (!checkBoxClass.empty()) {
                        pCheckBox->SetClass(checkBoxClass);
                    }
                }
                UiPadding textPadding = pLabelBox->GetTextPadding();
                int32_t nCheckBoxWidth = pStorage->nCheckBoxWidth;
                if (textPadding.left < nCheckBoxWidth) {
                    textPadding.left = nCheckBoxWidth;
                    pLabelBox->SetTextPadding(textPadding, false);
                }

                //挂载CheckBox的事件处理
                pCheckBox->DetachEvent(kEventSelect);
                pCheckBox->DetachEvent(kEventUnSelect);
                pCheckBox->SetSelected(pStorage->bChecked);
                size_t nColumnId = elementData.nColumnId;
                pCheckBox->AttachSelect([this, nColumnId, nElementIndex](const EventArgs& /*args*/) {
                    OnDataItemChecked(nElementIndex, nColumnId, true);
                    return true;
                    });
                pCheckBox->AttachUnSelect([this, nColumnId, nElementIndex](const EventArgs& /*args*/) {
                    OnDataItemChecked(nElementIndex, nColumnId, false);
                    return true;
                    });
            }
            else {
                if (pLabelBox->GetItemCount() > 0) {
                    CheckBox* pCheckBox = dynamic_cast<CheckBox*>(pLabelBox->GetItemAt(0));
                    if (pCheckBox != nullptr) {
                        pLabelBox->RemoveItemAt(0);
                    }
                }
            }
        }
        else {
            pLabelBox->SetTextStyle(defaultLabelBox.GetTextStyle(), false);
            pLabelBox->SetText(defaultLabelBox.GetText());
            pLabelBox->SetTextPadding(defaultLabelBox.GetTextPadding(), false);
            pLabelBox->SetStateTextColor(kControlStateNormal, defaultLabelBox.GetStateTextColor(kControlStateNormal));
            pLabelBox->SetBkColor(defaultLabelBox.GetBkColor());
            if (pLabelBox->GetItemCount() > 0) {
                CheckBox* pCheckBox = dynamic_cast<CheckBox*>(pLabelBox->GetItemAt(0));
                if (pCheckBox != nullptr) {
                    pLabelBox->RemoveItemAt(0);
                }
            }
        }

        //绘制边线
        int32_t mColumnLineWidth = GlobalManager::Instance().Dpi().GetScaleInt(1);//纵向边线宽度        
        int32_t mRowLineWidth = GlobalManager::Instance().Dpi().GetScaleInt(1);   //横向边线宽度
        if (m_pListCtrl != nullptr) {
            mColumnLineWidth = m_pListCtrl->GetColumnGridLineWidth();
            mRowLineWidth = m_pListCtrl->GetRowGridLineWidth();
        }
        if (bFirstLine) {
            //第一行
            if (nColumn == 0) {
                //第一列
                UiRect rc(mColumnLineWidth, mRowLineWidth, mColumnLineWidth, mRowLineWidth);
                pLabelBox->SetBorderSize(rc, false);
            }
            else {
                //非第一列
                UiRect rc(0, mRowLineWidth, mColumnLineWidth, mRowLineWidth);
                pLabelBox->SetBorderSize(rc, false);
            }
        }
        else {
            //非第一行
            if (nColumn == 0) {
                //第一列
                UiRect rc(mColumnLineWidth, 0, mColumnLineWidth, mRowLineWidth);
                pLabelBox->SetBorderSize(rc, false);
            }
            else {
                //非第一列
                UiRect rc(0, 0, mColumnLineWidth, mRowLineWidth);
                pLabelBox->SetBorderSize(rc, false);
            }
        }
    }
    return true;
}

size_t ListCtrlDataProvider::GetElementCount() const
{
    return GetDataItemCount();
}

void ListCtrlDataProvider::SetElementSelected(size_t nElementIndex, bool bSelected)
{
    if (nElementIndex == Box::InvalidIndex) {
        //如果选中的是Header，忽略
        return;
    }
    ASSERT(nElementIndex < m_rowDataList.size());
    if (nElementIndex < m_rowDataList.size()) {
        ListCtrlRowData& rowData = m_rowDataList[nElementIndex];
        rowData.bSelected = bSelected;
    }
}

bool ListCtrlDataProvider::IsElementSelected(size_t nElementIndex) const
{
    if (nElementIndex == Box::InvalidIndex) {
        //如果选中的是Header，忽略
        return false;
    }
    bool bSelected = false;
    ASSERT(nElementIndex < m_rowDataList.size());
    if (nElementIndex < m_rowDataList.size()) {
        const ListCtrlRowData& rowData = m_rowDataList[nElementIndex];
        bSelected = rowData.bSelected;
    }
    return bSelected;
}

void ListCtrlDataProvider::SetListCtrl(ListCtrl* pListCtrl)
{
    m_pListCtrl = pListCtrl;
}

void ListCtrlDataProvider::DataItemToStorage(const ListCtrlDataItem& item, Storage& storage) const
{
    storage.text = item.text;
    if (item.nTextFormat >= 0) {
        storage.nTextFormat = TruncateToUInt16(item.nTextFormat);
    }
    else {
        storage.nTextFormat = 0;
    }
    int32_t nCheckBoxWidth = item.nCheckBoxWidth;
    if (item.bNeedDpiScale) {
        GlobalManager::Instance().Dpi().ScaleInt(nCheckBoxWidth);
    }
    storage.nImageIndex = item.nImageIndex;
    storage.textColor = item.textColor;
    storage.bkColor = item.bkColor;
    storage.bShowCheckBox = item.bShowCheckBox;
    storage.nCheckBoxWidth = TruncateToUInt8(nCheckBoxWidth);
}

void ListCtrlDataProvider::StorageToDataItem(const Storage& storage, ListCtrlDataItem& item) const
{
    item.text = storage.text.c_str();
    if (storage.nTextFormat == 0) {
        item.nTextFormat = -1;
    }
    else {
        item.nTextFormat = storage.nTextFormat;
    }
    item.bNeedDpiScale = false;
    item.nCheckBoxWidth = storage.nCheckBoxWidth;    
    item.nImageIndex = storage.nImageIndex;
    item.textColor = storage.textColor;
    item.bkColor = storage.bkColor;
    item.bShowCheckBox = storage.bShowCheckBox;
}

size_t ListCtrlDataProvider::GetColumnId(size_t nColumnIndex) const
{
    ListCtrlHeader* pHeaderCtrl = nullptr;
    if (m_pListCtrl != nullptr) {
        pHeaderCtrl = m_pListCtrl->GetListCtrlHeader();
    }
    size_t columnId = Box::InvalidIndex;
    ASSERT(pHeaderCtrl != nullptr);
    if (pHeaderCtrl != nullptr) {
        columnId = pHeaderCtrl->GetColumnId(nColumnIndex);
    }     
    return columnId;
}

size_t ListCtrlDataProvider::GetColumnIndex(size_t nColumnId) const
{
    ListCtrlHeader* pHeaderCtrl = nullptr;
    if (m_pListCtrl != nullptr) {
        pHeaderCtrl = m_pListCtrl->GetListCtrlHeader();
    }
    size_t columnIndex = Box::InvalidIndex;
    ASSERT(pHeaderCtrl != nullptr);
    if (pHeaderCtrl != nullptr) {
        columnIndex = pHeaderCtrl->GetColumnIndex(nColumnId);
    }
    return columnIndex;
}

bool ListCtrlDataProvider::IsValidDataItemIndex(size_t itemIndex) const
{
    return itemIndex < m_rowDataList.size();
}

bool ListCtrlDataProvider::IsValidDataColumnId(size_t nColumnId) const
{
    return m_dataMap.find(nColumnId) != m_dataMap.end();
}

bool ListCtrlDataProvider::AddColumn(size_t columnId)
{
    ASSERT((columnId != Box::InvalidIndex) && (columnId != 0));
    if ((columnId == Box::InvalidIndex) || (columnId == 0)) {
        return false;
    }
    StoragePtrList& storageList = m_dataMap[columnId];
    //列的长度与行保持一致
    storageList.resize(m_rowDataList.size());
    EmitCountChanged();
    return true;
}

bool ListCtrlDataProvider::RemoveColumn(size_t columnId)
{
    auto iter = m_dataMap.find(columnId);
    if (iter != m_dataMap.end()) {
        m_dataMap.erase(iter);
        if (m_dataMap.empty()) {
            //如果所有列都删除了，行也清空为0
            m_rowDataList.clear();
            m_hideRowCount = 0;
            m_heightRowCount = 0;
            m_atTopRowCount = 0;
        }
        EmitCountChanged();
        return true;
    }
    return false;
}

bool ListCtrlDataProvider::SetColumnCheck(size_t columnId, bool bChecked)
{
    bool bRet = false;
    auto iter = m_dataMap.find(columnId);
    ASSERT(iter != m_dataMap.end());
    if (iter != m_dataMap.end()) {
        StoragePtrList& storageList = iter->second;
        const size_t nCount = storageList.size();
        for (size_t index = 0; index < nCount; ++index) {
            if (storageList[index] == nullptr) {
                storageList[index] = std::make_shared<Storage>();
            }
            storageList[index]->bChecked = bChecked;
        }
        bRet = true;
    }
    return bRet;
}

ListCtrlDataProvider::StoragePtr ListCtrlDataProvider::GetDataItemStorage(
    size_t itemIndex, size_t columnIndex) const
{
    ASSERT(IsValidDataItemIndex(itemIndex));
    if (!IsValidDataItemIndex(itemIndex)) {
        //索引号无效
        return nullptr;
    }
    StoragePtr pStorage;
    size_t columnId = GetColumnId(columnIndex);
    auto iter = m_dataMap.find(columnId);
    ASSERT(iter != m_dataMap.end());
    if (iter != m_dataMap.end()) {
        const StoragePtrList& storageList = iter->second;
        ASSERT(itemIndex < storageList.size());
        if (itemIndex < storageList.size()) {
            //关联列：获取数据
            pStorage = storageList[itemIndex];
        }
    }
    return pStorage;
}

ListCtrlDataProvider::StoragePtr ListCtrlDataProvider::GetDataItemStorageForWrite(
    size_t itemIndex, size_t columnIndex)
{
    ASSERT(IsValidDataItemIndex(itemIndex));
    if (!IsValidDataItemIndex(itemIndex)) {
        //索引号无效
        return nullptr;
    }
    StoragePtr pStorage;
    size_t columnId = GetColumnId(columnIndex);
    auto iter = m_dataMap.find(columnId);
    ASSERT(iter != m_dataMap.end());
    if (iter != m_dataMap.end()) {
        StoragePtrList& storageList = iter->second;
        ASSERT(itemIndex < storageList.size());
        if (itemIndex < storageList.size()) {
            //关联列：获取数据
            pStorage = storageList[itemIndex];
            if (pStorage == nullptr) {
                pStorage = std::make_shared<Storage>();
                storageList[itemIndex] = pStorage;
            }
        }
    }
    return pStorage;
}

bool ListCtrlDataProvider::GetDataItemStorageList(size_t nDataItemIndex, std::vector<size_t>& columnIdList,
                                                  StoragePtrList& storageList) const
{
    storageList.clear();
    ASSERT(nDataItemIndex != Box::InvalidIndex);
    if (nDataItemIndex == Box::InvalidIndex) {
        return false;
    }
    for (size_t nColumnId : columnIdList) {
        StoragePtr pStorage;
        auto iter = m_dataMap.find(nColumnId);        
        if (iter != m_dataMap.end()) {
            const StoragePtrList& dataList = iter->second;
            if (nDataItemIndex < dataList.size()) {
                pStorage = dataList.at(nDataItemIndex);
            }
        }
        storageList.push_back(pStorage);
    }
    return storageList.size() == columnIdList.size();
}

void ListCtrlDataProvider::OnDataItemChecked(size_t itemIndex, size_t nColumnId, bool bChecked)
{
    StoragePtr pStorage;
    auto iter = m_dataMap.find(nColumnId);    
    if (iter != m_dataMap.end()) {
        StoragePtrList& dataList = iter->second;
        ASSERT(itemIndex < dataList.size());
        if (itemIndex < dataList.size()) {
            pStorage = dataList.at(itemIndex);
            if (pStorage == nullptr) {
                pStorage = std::make_shared<Storage>();
                dataList[itemIndex] = pStorage;
            }
        }
    }
    if (pStorage != nullptr) {
        //更新存储状态
        pStorage->bChecked = bChecked;
    }

    //更新header的勾选状态（三态），仅仅同步UI的状态
    UpdateControlCheckStatus(nColumnId);
}

void ListCtrlDataProvider::UpdateControlCheckStatus(size_t nColumnId)
{
    if (m_pListCtrl != nullptr) {
        m_pListCtrl->UpdateControlCheckStatus(nColumnId);
    }
}

const ListCtrlDataProvider::RowDataList& ListCtrlDataProvider::GetItemDataList() const
{
    return m_rowDataList;
}

bool ListCtrlDataProvider::IsNormalMode() const
{
    ASSERT((m_hideRowCount >= 0) && (m_heightRowCount >= 0) && (m_atTopRowCount >= 0));
    return (m_hideRowCount == 0) && (m_heightRowCount == 0) && (m_atTopRowCount == 0);
}

size_t ListCtrlDataProvider::GetDataItemCount() const
{
#ifdef _DEBUG
    auto iter = m_dataMap.begin();
    for (; iter != m_dataMap.end(); ++iter) {
        ASSERT(iter->second.size() == m_rowDataList.size());
    }
#endif
    return m_rowDataList.size();
}

bool ListCtrlDataProvider::SetDataItemCount(size_t itemCount)
{
    ASSERT(itemCount != Box::InvalidIndex);
    if (itemCount == Box::InvalidIndex) {
        return false;
    }
    if (itemCount == m_rowDataList.size()) {
        //没有变化
        return true;
    }
    size_t nOldCount = m_rowDataList.size();
    m_rowDataList.resize(itemCount);    
    for (auto iter = m_dataMap.begin(); iter != m_dataMap.end(); ++iter) {
        iter->second.resize(itemCount);
    }
    if (itemCount < nOldCount) {
        //行数变少了
        if ((m_hideRowCount != 0) || (m_heightRowCount != 0) || (m_atTopRowCount != 0)) {
            UpdateNormalMode();
        }
    }
    EmitCountChanged();
    return true;
}

void ListCtrlDataProvider::UpdateNormalMode()
{
    m_hideRowCount = 0;
    m_heightRowCount = 0;
    m_atTopRowCount = 0;
    for (const ListCtrlRowData& data : m_rowDataList) {
        if (!data.bVisible) {
            m_hideRowCount += 1;
        }
        if (data.nItemHeight >= 0) {
            m_heightRowCount += 1;
        }
        if (data.nAlwaysAtTop >= 0) {
            m_atTopRowCount += 1;
        }
    }
}

size_t ListCtrlDataProvider::AddDataItem(const ListCtrlDataItem& dataItem)
{
    size_t columnId = GetColumnId(dataItem.nColumnIndex);
    ASSERT(IsValidDataColumnId(columnId));
    if (!IsValidDataColumnId(columnId)) {
        return Box::InvalidIndex;
    }

    Storage storage;
    DataItemToStorage(dataItem, storage);

    size_t nDataItemIndex = Box::InvalidIndex;
    for (auto iter = m_dataMap.begin(); iter != m_dataMap.end(); ++iter) {
        size_t id = iter->first;
        StoragePtrList& storageList = iter->second;
        if (id == columnId) {
            //关联列：保存数据
            storageList.push_back(std::make_shared<Storage>(storage));
            nDataItemIndex = storageList.size() - 1;
        }
        else {
            //其他列：插入空数据
            storageList.push_back(nullptr);
        }
    }

    //行数据，插入1条数据
    m_rowDataList.push_back(ListCtrlRowData());

    EmitCountChanged();
    return nDataItemIndex;
}

bool ListCtrlDataProvider::InsertDataItem(size_t itemIndex, const ListCtrlDataItem& dataItem)
{
    size_t columnId = GetColumnId(dataItem.nColumnIndex);
    ASSERT(IsValidDataColumnId(columnId));
    if (!IsValidDataColumnId(columnId)) {
        return false;
    }

    if (!IsValidDataItemIndex(itemIndex)) {
        //如果索引号无效，则按追加的方式添加数据
        return (AddDataItem(dataItem) != Box::InvalidIndex);
    }

    Storage storage;
    DataItemToStorage(dataItem, storage);

    for (auto iter = m_dataMap.begin(); iter != m_dataMap.end(); ++iter) {
        size_t id = iter->first;
        StoragePtrList& storageList = iter->second;
        if (id == columnId) {
            //关联列：保存数据
            storageList.insert(storageList.begin() + itemIndex, std::make_shared<Storage>(storage));
        }
        else {
            //其他列：插入空数据
            storageList.insert(storageList.begin() + itemIndex, nullptr);
        }
    }

    //行数据，插入1条数据
    ASSERT(itemIndex < m_rowDataList.size());
    m_rowDataList.insert(m_rowDataList.begin() + itemIndex, ListCtrlRowData());

    EmitCountChanged();
    return true;
}

bool ListCtrlDataProvider::SetDataItem(size_t itemIndex, const ListCtrlDataItem& dataItem)
{
    Storage storage;
    DataItemToStorage(dataItem, storage);

    bool bRet = false;
    size_t columnId = GetColumnId(dataItem.nColumnIndex);
    auto iter = m_dataMap.find(columnId);
    ASSERT(iter != m_dataMap.end());
    if (iter != m_dataMap.end()) {
        //关联列：更新数据
        StoragePtrList& storageList = iter->second;
        ASSERT(itemIndex < storageList.size());
        if (itemIndex < storageList.size()) {
            StoragePtr pStorage = storageList[itemIndex];
            if (pStorage == nullptr) {
                storageList[itemIndex] = std::make_shared<Storage>(storage);
            }
            else {
                *pStorage = storage;
            }
            bRet = true;
        }
    }

    if (bRet) {
        EmitDataChanged(itemIndex, itemIndex);
    }    
    return bRet;
}

bool ListCtrlDataProvider::GetDataItem(size_t itemIndex, size_t columnIndex, ListCtrlDataItem& dataItem) const
{
    dataItem = ListCtrlDataItem();
    dataItem.nColumnIndex = columnIndex;

    bool bRet = false;
    size_t columnId = GetColumnId(columnIndex);
    auto iter = m_dataMap.find(columnId);
    ASSERT(iter != m_dataMap.end());
    if (iter != m_dataMap.end()) {
        const StoragePtrList& storageList = iter->second;
        ASSERT(itemIndex < storageList.size());
        if (itemIndex < storageList.size()) {
            StoragePtr pStorage = storageList[itemIndex];
            if (pStorage != nullptr) {
                StorageToDataItem(*pStorage, dataItem);
            }
            bRet = true;
        }
    }
    return bRet;
}

bool ListCtrlDataProvider::DeleteDataItem(size_t itemIndex)
{
    if (!IsValidDataItemIndex(itemIndex)) {
        //索引号无效
        return false;
    }

    for (auto iter = m_dataMap.begin(); iter != m_dataMap.end(); ++iter) {
        StoragePtrList& storageList = iter->second;
        if (itemIndex < storageList.size()) {
            storageList.erase(storageList.begin() + itemIndex);
        }
    }

    //删除一行
    if (itemIndex < m_rowDataList.size()) {
        ListCtrlRowData oldData = m_rowDataList[itemIndex];
        m_rowDataList.erase(m_rowDataList.begin() + itemIndex);
        if (!oldData.bVisible) {
            m_hideRowCount -= 1;
            ASSERT(m_hideRowCount >= 0);
        }
        if (oldData.nItemHeight >= 0) {
            m_heightRowCount -= 1;
            ASSERT(m_heightRowCount >= 0);
        }
        if (oldData.nAlwaysAtTop >= 0) {
            m_atTopRowCount -= 1;
            ASSERT(m_atTopRowCount >= 0);
        }
    }
    EmitCountChanged();
    return true;
}

bool ListCtrlDataProvider::DeleteAllDataItems()
{
    bool bDeleted = false;
    for (auto iter = m_dataMap.begin(); iter != m_dataMap.end(); ++iter) {
        StoragePtrList& storageList = iter->second;
        if (!storageList.empty()) {
            bDeleted = true;
        }
        StoragePtrList emptyList;
        storageList.swap(emptyList);
    }
    //清空行数据
    if (!m_rowDataList.empty()) {
        bDeleted = true;
    }
    m_rowDataList.clear();
    m_hideRowCount = 0;
    m_heightRowCount = 0;
    m_atTopRowCount = 0;

    if (bDeleted) {
        EmitCountChanged();
    }
    return bDeleted;
}

bool ListCtrlDataProvider::SetDataItemRowData(size_t itemIndex, const ListCtrlRowData& itemData, bool& bChanged)
{
    bChanged = false;
    bool bRet = false;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        const ListCtrlRowData oldItemData = m_rowDataList[itemIndex];
        m_rowDataList[itemIndex] = itemData;
        if (m_pListCtrl != nullptr) {
            if (m_pListCtrl->GetDataItemHeight() == m_rowDataList[itemIndex].nItemHeight) {
                //如果等于默认高度，则设置为标志值
                m_rowDataList[itemIndex].nItemHeight = -1;
            }
        }
        const ListCtrlRowData& newItemData = m_rowDataList[itemIndex];
        if (newItemData.bSelected != oldItemData.bSelected) {
            bChanged = true;
        }
        else if (newItemData.bVisible != oldItemData.bVisible) {
            bChanged = true;
        }
        else if (newItemData.nAlwaysAtTop != oldItemData.nAlwaysAtTop) {
            bChanged = true;
        }
        else if (newItemData.nItemHeight != oldItemData.nItemHeight) {
            bChanged = true;
        }
        else if (newItemData.nItemData != oldItemData.nItemData) {
            bChanged = true;
        }

        //更新计数
        if (!oldItemData.bVisible && newItemData.bVisible) {
            m_hideRowCount -= 1;            
        }
        else if (oldItemData.bVisible && !newItemData.bVisible) {
            m_hideRowCount += 1;
        }
        ASSERT(m_hideRowCount >= 0);

        if ((oldItemData.nItemHeight >= 0) && (newItemData.nItemHeight < 0)) {
            m_heightRowCount -= 1;
        }
        else if ((oldItemData.nItemHeight < 0) && (newItemData.nItemHeight >= 0)) {
            m_heightRowCount += 1;
        }
        ASSERT(m_heightRowCount >= 0);

        if ((oldItemData.nAlwaysAtTop >= 0) && (newItemData.nAlwaysAtTop < 0)) {
            m_atTopRowCount -= 1;
        }
        else if ((oldItemData.nAlwaysAtTop < 0) && (newItemData.nAlwaysAtTop >= 0)) {
            m_atTopRowCount += 1;
        }
        ASSERT(m_atTopRowCount >= 0);
        bRet = true;
    }
    return bRet;
}

bool ListCtrlDataProvider::GetDataItemRowData(size_t itemIndex, ListCtrlRowData& itemData) const
{
    bool bRet = false;
    itemData = ListCtrlRowData();
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        itemData = m_rowDataList[itemIndex];
        bRet = true;
    }
    return bRet;
}

bool ListCtrlDataProvider::SetDataItemVisible(size_t itemIndex, bool bVisible, bool& bChanged)
{
    bChanged = false;
    bool bRet = false;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {        
        ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        bool bOldVisible = rowData.bVisible;
        bChanged = rowData.bVisible != bVisible;
        rowData.bVisible = bVisible;

        if (!bOldVisible && bVisible) {
            m_hideRowCount -= 1;
        }
        else if (bOldVisible && !bVisible) {
            m_hideRowCount += 1;
        }
        ASSERT(m_hideRowCount >= 0);
        bRet = true;
    } 

    //不刷新，由外部判断是否需要刷新
    return bRet;
}

bool ListCtrlDataProvider::IsDataItemVisible(size_t itemIndex) const
{
    bool bValue = false;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        const ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        bValue = rowData.bVisible;
    }
    return bValue;
}

bool ListCtrlDataProvider::SetDataItemSelected(size_t itemIndex, bool bSelected, bool& bChanged)
{
    bChanged = false;
    bool bRet = false;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        bChanged = rowData.bSelected != bSelected;
        rowData.bSelected = bSelected;
        bRet = true;
    }
    //不刷新，由外部判断是否需要刷新
    return bRet;
}

bool ListCtrlDataProvider::IsDataItemSelected(size_t itemIndex) const
{
    bool bValue = false;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        const ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        bValue = rowData.bSelected;
    }
    return bValue;
}

bool ListCtrlDataProvider::SetDataItemAlwaysAtTop(size_t itemIndex, int8_t nAlwaysAtTop, bool& bChanged)
{
    bChanged = false;
    bool bRet = false;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        int8_t nOldAlwaysAtTop = rowData.nAlwaysAtTop;
        bChanged = rowData.nAlwaysAtTop != nAlwaysAtTop;
        rowData.nAlwaysAtTop = nAlwaysAtTop;
        if ((nOldAlwaysAtTop >= 0) && (nAlwaysAtTop < 0)) {
            m_atTopRowCount -= 1;
        }
        else if ((nOldAlwaysAtTop < 0) && (nAlwaysAtTop >= 0)) {
            m_atTopRowCount += 1;
        }
        ASSERT(m_atTopRowCount >= 0);
        bRet = true;
    }
    //不刷新，由外部判断是否需要刷新
    return bRet;
}

int8_t ListCtrlDataProvider::GetDataItemAlwaysAtTop(size_t itemIndex) const
{
    int8_t nValue = -1;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        const ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        nValue = rowData.nAlwaysAtTop;
    }
    return nValue;
}

bool ListCtrlDataProvider::SetDataItemHeight(size_t itemIndex, int32_t nItemHeight, bool bNeedDpiScale, bool& bChanged)
{
    bChanged = false;
    if (nItemHeight < 0) {
        nItemHeight = -1;
    }
    if (bNeedDpiScale && (nItemHeight > 0)) {
        GlobalManager::Instance().Dpi().ScaleInt(nItemHeight);
    }
    if (m_pListCtrl != nullptr) {
        if (m_pListCtrl->GetDataItemHeight() == nItemHeight) {
            //如果等于默认高度，则设置为标志值
            nItemHeight = -1;
        }
    }
    bool bRet = false;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        int16_t nOldItemHeight = rowData.nItemHeight;
        bChanged = rowData.nItemHeight != nItemHeight;
        ASSERT(nItemHeight <= INT16_MAX);
        rowData.nItemHeight = (int16_t)nItemHeight;
        if ((nOldItemHeight >= 0) && (nItemHeight < 0)) {
            m_heightRowCount -= 1;
        }
        else if ((nOldItemHeight < 0) && (nItemHeight >= 0)) {
            m_heightRowCount += 1;
        }
        ASSERT(m_heightRowCount >= 0);
        bRet = true;
    }
    //不刷新，由外部判断是否需要刷新
    return bRet;
}

int32_t ListCtrlDataProvider::GetDataItemHeight(size_t itemIndex) const
{
    int32_t nValue = 0;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        const ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        nValue = rowData.nItemHeight;
        if ((nValue < 0) && (m_pListCtrl != nullptr)) {
            //取默认高度
            nValue = m_pListCtrl->GetDataItemHeight();
        }
    }
    return nValue;
}

bool ListCtrlDataProvider::SetDataItemData(size_t itemIndex, size_t itemData)
{
    bool bRet = false;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        rowData.nItemData = itemData;
        bRet = true;
    }
    return bRet;
}

size_t ListCtrlDataProvider::GetDataItemData(size_t itemIndex) const
{
    size_t nItemData = 0;
    ASSERT(itemIndex < m_rowDataList.size());
    if (itemIndex < m_rowDataList.size()) {
        const ListCtrlRowData& rowData = m_rowDataList[itemIndex];
        nItemData = rowData.nItemData;
    }
    return nItemData;
}

bool ListCtrlDataProvider::SetDataItemText(size_t itemIndex, size_t columnIndex, const std::wstring& text)
{
    StoragePtr pStorage = GetDataItemStorageForWrite(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    if (pStorage->text != text) {
        pStorage->text = text;
        EmitDataChanged(itemIndex, itemIndex);
    }    
    return true;
}

std::wstring ListCtrlDataProvider::GetDataItemText(size_t itemIndex, size_t columnIndex) const
{
    StoragePtr pStorage = GetDataItemStorage(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return std::wstring();
    }
    return pStorage->text.c_str();
}

bool ListCtrlDataProvider::SetDataItemTextColor(size_t itemIndex, size_t columnIndex, const UiColor& textColor)
{
    StoragePtr pStorage = GetDataItemStorageForWrite(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    if (pStorage->textColor != textColor) {
        pStorage->textColor = textColor;
        EmitDataChanged(itemIndex, itemIndex);
    }    
    return true;
}

bool ListCtrlDataProvider::GetDataItemTextColor(size_t itemIndex, size_t columnIndex, UiColor& textColor) const
{
    textColor = UiColor();
    StoragePtr pStorage = GetDataItemStorage(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    textColor = pStorage->textColor;
    return true;
}

bool ListCtrlDataProvider::SetDataItemBkColor(size_t itemIndex, size_t columnIndex, const UiColor& bkColor)
{
    StoragePtr pStorage = GetDataItemStorageForWrite(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    if (pStorage->bkColor != bkColor) {
        pStorage->bkColor = bkColor;
        EmitDataChanged(itemIndex, itemIndex);
    }    
    return true;
}

bool ListCtrlDataProvider::GetDataItemBkColor(size_t itemIndex, size_t columnIndex, UiColor& bkColor) const
{
    bkColor = UiColor();
    StoragePtr pStorage = GetDataItemStorage(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    bkColor = pStorage->bkColor;
    return true;
}

bool ListCtrlDataProvider::IsShowCheckBox(size_t itemIndex, size_t columnIndex) const
{
    StoragePtr pStorage = GetDataItemStorage(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    return pStorage->bShowCheckBox;
}

bool ListCtrlDataProvider::SetShowCheckBox(size_t itemIndex, size_t columnIndex, bool bShowCheckBox)
{
    StoragePtr pStorage = GetDataItemStorageForWrite(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    if (pStorage->bShowCheckBox != bShowCheckBox) {
        pStorage->bShowCheckBox = bShowCheckBox;
        EmitDataChanged(itemIndex, itemIndex);
    }    
    return true;
}

bool ListCtrlDataProvider::SetCheckBoxSelect(size_t itemIndex, size_t columnIndex, bool bSelected)
{
    StoragePtr pStorage = GetDataItemStorageForWrite(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    ASSERT(pStorage->bShowCheckBox);
    if (pStorage->bShowCheckBox) {
        if (pStorage->bChecked != bSelected) {
            pStorage->bChecked = bSelected;
            EmitDataChanged(itemIndex, itemIndex);
        }        
        return true;
    }
    return false;
}

bool ListCtrlDataProvider::GetCheckBoxSelect(size_t itemIndex, size_t columnIndex, bool& bSelected) const
{
    bSelected = false;
    StoragePtr pStorage = GetDataItemStorage(itemIndex, columnIndex);
    ASSERT(pStorage != nullptr);
    if (pStorage == nullptr) {
        //索引号无效
        return false;
    }
    ASSERT(pStorage->bShowCheckBox);
    if (pStorage->bShowCheckBox) {
        bSelected = pStorage->bChecked;
        return true;
    }
    return false;
}

bool ListCtrlDataProvider::SortDataItems(size_t nColumnId, bool bSortedUp, 
                                         ListCtrlDataCompareFunc pfnCompareFunc, void* pUserData)
{
    StorageMap::iterator iter = m_dataMap.find(nColumnId);
    ASSERT(iter != m_dataMap.end());
    if (iter == m_dataMap.end()) {
        return false;
    }
    StoragePtrList sortStorageList = iter->second;
    if (sortStorageList.empty()) {
        return false;
    }
    std::vector<StorageData> sortedDataList;
    const size_t dataCount = sortStorageList.size();
    for (size_t index = 0; index < dataCount; ++index) {
        sortedDataList.push_back({index, sortStorageList[index] });
    }    
    SortStorageData(sortedDataList, nColumnId, bSortedUp, pfnCompareFunc, pUserData);

    //对原数据进行顺序调整
    const size_t sortedDataCount = sortedDataList.size();
    StoragePtrList orgStorageList; //副本数据
    for (iter = m_dataMap.begin(); iter != m_dataMap.end(); ++iter) {
        orgStorageList = iter->second; 
        StoragePtrList& storageList = iter->second;   //修改目标
        ASSERT(storageList.size() == sortedDataList.size());
        if (storageList.size() != sortedDataList.size()) {
            return false;
        }
        for (size_t index = 0; index < sortedDataCount; ++index) {
            const StorageData& sortedData = sortedDataList[index];
            storageList[index] = orgStorageList[sortedData.index]; //赋值原数据
        }
    }

    //对行数据进行排序
    ASSERT(sortedDataCount == m_rowDataList.size());
    RowDataList rowDataList = m_rowDataList;
    for (size_t index = 0; index < sortedDataCount; ++index) {
        const StorageData& sortedData = sortedDataList[index];
        m_rowDataList[index] = rowDataList[sortedData.index]; //赋值原数据
    }
    return true;
}

bool ListCtrlDataProvider::SortStorageData(std::vector<StorageData>& dataList,                                            
                                           size_t nColumnId, bool bSortedUp,
                                           ListCtrlDataCompareFunc pfnCompareFunc,
                                           void* pUserData)
{
    if (dataList.empty()) {
        return false;
    }

    if (pfnCompareFunc == nullptr) {
        //如果无有效参数，则使用外部设置的排序函数
        pfnCompareFunc = m_pfnCompareFunc;
        pUserData = m_pUserData;
    }

    if (pfnCompareFunc != nullptr) {
        //使用自定义的比较函数排序
        ListCtrlCompareParam param;
        param.nColumnId = nColumnId;
        param.nColumnIndex = Box::InvalidIndex;
        param.pUserData = pUserData;
        if (m_pListCtrl != nullptr) {
            param.nColumnIndex = m_pListCtrl->GetColumnIndex(nColumnId);
        }
        std::sort(dataList.begin(), dataList.end(), [this, pfnCompareFunc, &param](const StorageData& a, const StorageData& b) {
                //实现(a < b)的比较逻辑
                if (b.pStorage == nullptr) {
                    return false;
                }
                if (a.pStorage == nullptr) {
                    return true;
                }
                const Storage& storageA = *a.pStorage;
                const Storage& storageB = *b.pStorage;
                return pfnCompareFunc(storageA, storageB, param);
            });
    }
    else {
        //排序：升序，使用默认的排序函数
        std::sort(dataList.begin(), dataList.end(), [this](const StorageData& a, const StorageData& b) {
                //实现(a < b)的比较逻辑
                if (b.pStorage == nullptr) {
                    return false;
                }
                if (a.pStorage == nullptr) {
                    return true;
                }
                const Storage& storageA = *a.pStorage;
                const Storage& storageB = *b.pStorage;
                return SortDataCompareFunc(storageA, storageB);
            });
    }
    if (!bSortedUp) {
        //降序
        std::reverse(dataList.begin(), dataList.end());
    }
    return true;
}

bool ListCtrlDataProvider::SortDataCompareFunc(const ListCtrlData& a, const ListCtrlData& b) const
{
    //默认按字符串比较, 区分大小写
    return ::wcscmp(a.text.c_str(), b.text.c_str()) < 0;
}

void ListCtrlDataProvider::SetSortCompareFunction(ListCtrlDataCompareFunc pfnCompareFunc, void* pUserData)
{
    m_pfnCompareFunc = pfnCompareFunc;
    m_pUserData = pUserData;
}

}//namespace ui

