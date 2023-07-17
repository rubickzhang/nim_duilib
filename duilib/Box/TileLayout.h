#ifndef UI_BOX_TILELAYOUT_H_
#define UI_BOX_TILELAYOUT_H_

#pragma once

#include "duilib/Box/Layout.h"

namespace ui 
{

class UILIB_API TileLayout : public Layout
{
public:
	TileLayout();

	/// 重写父类方法，提供个性化功能，请参考父类声明
	virtual UiSize64 ArrangeChild(const std::vector<Control*>& items, UiRect rc) override;
	virtual UiSize EstimateSizeByChild(const std::vector<Control*>& items, UiSize szAvailable) override;
	virtual bool SetAttribute(const std::wstring& strName, const std::wstring& strValue) override;

	/**
	 * @brief 获取子项大小
	 * @return 返回子项大小
	 */
	const UiSize& GetItemSize() const;

	/**
	 * @brief 设置子项大小
	 * @param[in] szItem 子项大小数据
	 * @return 无
	 */
	void SetItemSize(UiSize szItem, bool bNeedDpiScale = true);

	/* @brief 获取列数量
	 */
	int GetColumns() const;

	/**@brief 设置显示几列数据
	 * @param[in] nCols 要设置显示几列的数值
	 */
	void SetColumns(int nCols);

protected:

	//显示几列数据
	int m_nColumns;

	//子项大小
	UiSize m_szItem;
};

} // namespace ui

#endif // UI_BOX_TILELAYOUT_H_
