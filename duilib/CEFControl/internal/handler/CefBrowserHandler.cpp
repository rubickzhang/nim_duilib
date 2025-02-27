#include "CefBrowserHandler.h"
#include "duilib/CEFControl/CefBrowserHandlerDelegate.h"
#include "duilib/CEFControl/CefManager.h"
#include "duilib/CEFControl/internal/app/CefIPCStringDefs.h"
#include "duilib/CEFControl/internal/app/CefJSBridge.h"

#include "duilib/Core/GlobalManager.h"

#pragma warning (push)
#pragma warning (disable:4100 4324)
#include "include/base/cef_callback.h"
#include "include/base/cef_bind.h"
#include "include/wrapper/cef_closure_task.h"

namespace ui
{
CefBrowserHandler::CefBrowserHandler()
{
    m_pWindow = nullptr;
    m_pHandleDelegate = nullptr;
    m_bFocusOnEditableField = false;
}

void CefBrowserHandler::SetHostWindow(ui::Window* window)
{ 
    m_pWindow = window;
    m_windowFlag.reset();
    if (window != nullptr) {
        m_windowFlag = window->GetWeakFlag();
    }
}

void CefBrowserHandler::SetViewRect(const UiRect& rc)
{
    if (!CefCurrentlyOn(TID_UI)) {
        // 把操作跳转到Cef线程执行后续设置
        {
            base::AutoLock lock_scope(lock_);
            m_rcCefControl = rc;//此处需要设置，否则首次绑定时，CefPostTask还未执行时，m_rcCefControl就会被读取，0值会触发错误
        }        
        CefPostTask(TID_UI, base::BindOnce(&CefBrowserHandler::SetViewRect, this, rc));
        return;
    }
    {
        base::AutoLock lock_scope(lock_);
        m_rcCefControl = rc;
    }
    // 调用WasResized接口，调用后，CefBrowserHandler会调用GetViewRect接口来获取浏览器对象新的位置
    if (m_browser.get() && m_browser->GetHost().get()) {
        m_browser->GetHost()->WasResized();
    }
}

CefRefPtr<CefBrowserHost> CefBrowserHandler::GetBrowserHost()
{
    if (m_browser.get()) {
        return m_browser->GetHost();
    }
    return nullptr;
}

CefUnregisterCallback CefBrowserHandler::AddAfterCreateTask(const ui::StdClosure& cb)
{
    return m_taskListAfterCreated.AddCallback(cb);
}

void CefBrowserHandler::CloseAllBrowser()
{
    class CloseAllBrowserTask : public CefTask
    {
        IMPLEMENT_REFCOUNTING(CloseAllBrowserTask);
    public:
        explicit CloseAllBrowserTask(const std::vector<CefRefPtr<CefBrowser>>& browser_list)
        {
            m_browserList.assign(browser_list.begin(), browser_list.end());
        }
    public:
        void Execute()
        {
            for (auto it : m_browserList) {
                if ((it != nullptr) && (it->GetHost() != nullptr)) {
                    it->GetHost()->CloseBrowser(true);
                }
            }
        }
    private:
        std::vector<CefRefPtr<CefBrowser>> m_browserList;
    };
    CefPostTask(TID_UI, new CloseAllBrowserTask(m_browserList));
}

bool CefBrowserHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                                 CefRefPtr<CefFrame> frame,
                                                 CefProcessId /*source_process*/,
                                                 CefRefPtr<CefProcessMessage> message)
{
    // 处理render进程发来的消息
    std::string message_name = message->GetName();
    if (message_name == kFocusedNodeChangedMessage) {
        m_bFocusOnEditableField = message->GetArgumentList()->GetBool(0);
        return true;
    }
    else if (message_name == kCallCppFunctionMessage) {
        CefString fun_name    = message->GetArgumentList()->GetString(0);
        CefString param        = message->GetArgumentList()->GetString(1);
        int js_callback_id    = message->GetArgumentList()->GetInt(2);

        if (m_pHandleDelegate) {
            m_pHandleDelegate->OnExecuteCppFunc(fun_name, param, js_callback_id, browser, frame);
        }
        return true;
    }
    else if (message_name == kExecuteCppCallbackMessage) {
        CefString param = message->GetArgumentList()->GetString(0);
        int callback_id = message->GetArgumentList()->GetInt(1);

        if (m_pHandleDelegate) {
            m_pHandleDelegate->OnExecuteCppCallbackFunc(callback_id, param);
        }
    }
    return false;
}

bool CefBrowserHandler::DoOnBeforePopup(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        int popup_id,
                                        const CefString& target_url,
                                        const CefString& target_frame_name,
                                        CefLifeSpanHandler::WindowOpenDisposition target_disposition,
                                        bool user_gesture,
                                        const CefPopupFeatures& popupFeatures,
                                        CefWindowInfo& windowInfo,
                                        CefRefPtr<CefClient>& client,
                                        CefBrowserSettings& settings,
                                        CefRefPtr<CefDictionaryValue>& extra_info,
                                        bool* no_javascript_access)
{
    // 让新的链接在原浏览器对象中打开
    if (m_browser.get() && !target_url.empty()) {
        if (m_pHandleDelegate) {
            // 返回true则继续在控件内打开新链接，false则禁止访问
            bool bRet = m_pHandleDelegate->OnBeforePopup(browser, frame, popup_id, target_url, target_frame_name, target_disposition, user_gesture, popupFeatures, windowInfo, client, settings, no_javascript_access);
            if (bRet) {
                if (m_browser->GetMainFrame() != nullptr) {
                    m_browser->GetMainFrame()->LoadURL(target_url);
                }
            }
        }
        else {
            if (m_browser->GetMainFrame() != nullptr) {
                m_browser->GetMainFrame()->LoadURL(target_url);
            }
        }
    }

    // 禁止弹出popup窗口
    return true;
}

#if CEF_VERSION_MAJOR > 109
//CEF 高版本
// CefLifeSpanHandler methods
bool CefBrowserHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      int /*popup_id*/,
                                      const CefString& target_url,
                                      const CefString& target_frame_name,
                                      CefLifeSpanHandler::WindowOpenDisposition target_disposition,
                                      bool user_gesture,
                                      const CefPopupFeatures& popupFeatures,
                                      CefWindowInfo& windowInfo,
                                      CefRefPtr<CefClient>& client,
                                      CefBrowserSettings& settings,
                                      CefRefPtr<CefDictionaryValue>& /*extra_info*/,
                                      bool* no_javascript_access)
{
    return DoOnBeforePopup(browser, frame, popup_id,
                           target_url, target_frame_name, target_disposition,
                           user_gesture, popupFeatures, windowInfo,
                           client, settings, extra_info,
                           no_javascript_access);
}

void CefBrowserHandler::OnBeforePopupAborted(CefRefPtr<CefBrowser> /*browser*/, int /*popup_id*/)
{
}

void CefBrowserHandler::OnBeforeDevToolsPopup(CefRefPtr<CefBrowser> /*browser*/,
                                              CefWindowInfo& /*windowInfo*/,
                                              CefRefPtr<CefClient>& /*client*/,
                                              CefBrowserSettings& /*settings*/,
                                              CefRefPtr<CefDictionaryValue>& /*extra_info*/,
                                              bool* /*use_default_window*/)
{
}
#else
bool CefBrowserHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      const CefString& target_url,
                                      const CefString& target_frame_name,
                                      CefLifeSpanHandler::WindowOpenDisposition target_disposition,
                                      bool user_gesture,
                                      const CefPopupFeatures& popupFeatures,
                                      CefWindowInfo& windowInfo,
                                      CefRefPtr<CefClient>& client,
                                      CefBrowserSettings& settings,
                                      CefRefPtr<CefDictionaryValue>& extra_info,
                                      bool* no_javascript_access)
{
    return DoOnBeforePopup(browser, frame, 0,
                           target_url, target_frame_name, target_disposition,
                           user_gesture, popupFeatures, windowInfo,
                           client, settings, extra_info,
                           no_javascript_access);
}
#endif

void CefBrowserHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    ASSERT(CefCurrentlyOn(TID_UI));
    ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, ToWeakCallback([this, browser](){
        m_browserList.emplace_back(browser);
        if ((m_browser != nullptr) && (m_browser->GetHost() != nullptr)) {
            m_browser->GetHost()->WasHidden(true);
        }
        m_browser = browser;
        CefManager::GetInstance()->AddBrowserCount();

        if (m_pHandleDelegate) {
            m_pHandleDelegate->OnAfterCreated(browser);
        }

        // 有窗模式下，浏览器创建完毕后，让上层更新一下自己的位置；因为在异步状态下，上层更新位置时可能Cef窗口还没有创建出来
        if (!CefManager::GetInstance()->IsEnableOffsetRender() && m_pHandleDelegate) {
            m_pHandleDelegate->UpdateWindowPos();
        }

        m_taskListAfterCreated();
        m_taskListAfterCreated.Clear();
    }));

    // 必须在cef ui线程调用RegisterDragDrop
    // 执行::DoDragDrop时，会在调用RegisterDragDrop的线程触发的DragOver、DragLeave、Drop、Drop回调
    // 进而调用browser_->GetHost()->DragTargetDragEnter、DragTargetDragOver、DragTargetDragLeave、DragTargetDrop
    // 这几个cef接口内部发现不在cef ui线程触发，则会转发到cef ui线程
    // 导致DragSourceEndedAt接口被调用时有部分DragTarget*方法没有被调用
    // 最终拖拽效果就会有问题，详见DragSourceEndedAt接口描述
    // 所以在cef ui线程调用RegisterDragDrop，让后面一系列操作都在cef ui线程里同步执行，则没问题
    //
    // RegisterDragDrop内部会在调用这个API的线程里创建一个窗口，用过这个窗口来做消息循环模拟阻塞的过程
    // 所以哪个线程调用RegisterDragDrop，就会在哪个线程阻塞并触发IDragTarget回调
    // 见https://docs.microsoft.com/zh-cn/windows/win32/api/ole2/nf-ole2-registerdragdrop
    if ((m_pWindow != nullptr) && !m_windowFlag.expired()) {
        m_dropTarget = CefManager::GetInstance()->GetDropTarget(m_pWindow->NativeWnd()->GetHWND());
    }
}

bool CefBrowserHandler::DoClose(CefRefPtr<CefBrowser> browser)
{
    return false;
}

void CefBrowserHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
    ASSERT(CefCurrentlyOn(TID_UI));
    ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, ToWeakCallback([this, browser](){
        CefManager::GetInstance()->SubBrowserCount();
        auto it = std::find_if(m_browserList.begin(), m_browserList.end(), [&](const CefRefPtr<CefBrowser>& item){
            return item->IsSame(browser);
        });
        if (it != m_browserList.end()) {
            auto closed_browser = *it;
            m_browserList.erase(it);
            if (closed_browser->IsSame(m_browser)) {
                m_browser = m_browserList.size() > 0 ? *m_browserList.rbegin() : nullptr;
                if (m_browser != nullptr)  {
                    if (m_browser->GetHost() != nullptr) {
                        m_browser->GetHost()->WasHidden(false);
                    }
                    m_browser->Reload();
                }
            }
        }

        if (m_pHandleDelegate) {
            m_pHandleDelegate->OnBeforeClose(browser);
        }
    }));
}

// CefRenderHandler methods

CefRefPtr<CefAccessibilityHandler> CefBrowserHandler::GetAccessibilityHandler()
{
    return nullptr;
}

bool CefBrowserHandler::GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
    if ((m_pWindow == nullptr) || m_windowFlag.expired()) {
        return false;
    }
    RECT window_rect = { 0 };
    HWND root_window = GetAncestor(m_pWindow->NativeWnd()->GetHWND(), GA_ROOT);
    if (::GetWindowRect(root_window, &window_rect)) {
        rect = CefRect(window_rect.left, window_rect.top, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top);
        return true;
    }
    return false;
}

void CefBrowserHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
    if ((m_pWindow == nullptr) || m_windowFlag.expired()) {
        return;
    }
    if (m_pHandleDelegate) {
        UiRect rect_cef_control;
        {
            base::AutoLock lock_scope(lock_);
            rect_cef_control = m_rcCefControl;
        }
        rect.x = 0;
        rect.y = 0;
        rect.width = rect_cef_control.right - rect_cef_control.left;
        rect.height = rect_cef_control.bottom - rect_cef_control.top;

        //返回的区域大小不能为0，否则返回会导致程序崩溃
        if (rect.width == 0)  {
            ui::UiRect clientRect;
            m_pWindow->GetClientRect(clientRect);
            rect.width = clientRect.Width();
        }
        if (rect.height == 0) {
            ui::UiRect clientRect;
            m_pWindow->GetClientRect(clientRect);
            rect.height = clientRect.Height();
        }

        if (CefManager::GetInstance()->IsEnableOffsetRender()) {
            //离屏渲染模式，需要传给原始宽度和高度，因为CEF内部会进一步做DPI自适应
            uint32_t dpiScale = m_pWindow->Dpi().GetScale();
            if (dpiScale > 100) {
                rect.width = rect.width * 100 / dpiScale;
                rect.height = rect.height * 100 / dpiScale;
            }
        }
    }
    else  {
        ui::UiRect clientRect;
        m_pWindow->GetClientRect(clientRect);
        rect.x = rect.y = 0;
        rect.width = clientRect.right;
        rect.height = clientRect.bottom;
    }

}

bool CefBrowserHandler::GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX, int viewY, int& screenX, int& screenY)
{
    if ((m_pWindow == nullptr) || m_windowFlag.expired()) {
        return false;
    }
    if (!m_pWindow->IsWindow()) {
        return false;
    }

    // Convert the point from view coordinates to actual screen coordinates.
    ui::UiPoint screen_pt = { viewX, viewY};
    if (CefManager::GetInstance()->IsEnableOffsetRender()) {
        //离屏渲染模式下，给到的参数是原始坐标，未经DPI自适应，所以需要做DPI自适应处理，否则页面的右键菜单位置显示不对
        uint32_t dpiScale = m_pWindow->Dpi().GetScale();
        if (dpiScale > 100) {
            screen_pt.x = screen_pt.x * dpiScale / 100;
            screen_pt.y = screen_pt.y * dpiScale / 100;
        }
    }
    //将页面坐标转换为窗口客户区坐标，否则页面弹出的右键菜单位置不正确
    UiRect rect_cef_control;
    {
        base::AutoLock lock_scope(lock_);
        rect_cef_control = m_rcCefControl;
    }
    screen_pt.x = screen_pt.x + rect_cef_control.left;
    screen_pt.y = screen_pt.y + rect_cef_control.top;
    m_pWindow->ClientToScreen(screen_pt);
    screenX = screen_pt.x;
    screenY = screen_pt.y;
    return true;
}

bool CefBrowserHandler::GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info)
{
    //只有离屏渲染模式下，才处理DPI缩放因子
    if (!CefManager::GetInstance()->IsEnableOffsetRender()) {
        return false;
    }
    if ((m_pWindow == nullptr) || m_windowFlag.expired()) {
        return false;
    }
    uint32_t dpiScale = m_pWindow->Dpi().GetScale();
    if (dpiScale == 100) {
        return false;
    }
    else {
        screen_info.device_scale_factor = dpiScale / 100.0f;
        return true;
    }    
}

void CefBrowserHandler::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show)
{
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnPopupShow, m_pHandleDelegate, browser, show));
    }
}

void CefBrowserHandler::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect)
{
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnPopupSize, m_pHandleDelegate, browser, rect));
    }
}

void CefBrowserHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                                PaintElementType type,
                                const RectList& dirtyRects,
                                const void* buffer,
                                int width,
                                int height)
{
    if (m_pHandleDelegate) {
        // 多线程消息循环模式中，OnPaint在Cef的线程被触发，这时把数据保存到paint_buffer_中，跳转到UI线程执行渲染操作。
        // 这里不对paint_buffer_加锁，即使两个线程操作paint_buffer_发生竞争，也只是让某一次渲染效果有瑕疵，不会崩溃，这个瑕疵是可以接受的
        int buffer_length = width * height * 4;
        if (buffer_length > (int)m_paintBuffer.size()) {
            m_paintBuffer.resize(buffer_length + 1);
        }
        memcpy(&m_paintBuffer[0], (char*)buffer, width * height * 4);
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnPaint, m_pHandleDelegate, browser, type, dirtyRects, &m_paintBuffer, width, height));
    }
}
#if CEF_VERSION_MAJOR <= 109
//CEF 109版本
void CefBrowserHandler::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, void* shared_handle)
{
}
#else
//CEF 高版本
void CefBrowserHandler::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const CefAcceleratedPaintInfo& info)
{
}
#endif

void CefBrowserHandler::GetTouchHandleSize(CefRefPtr<CefBrowser> browser,
                                        cef_horizontal_alignment_t orientation,
                                        CefSize& size) {
}

void CefBrowserHandler::OnTouchHandleStateChanged(CefRefPtr<CefBrowser> browser, const CefTouchHandleState& state)
{
}

void CefBrowserHandler::OnScrollOffsetChanged(CefRefPtr<CefBrowser> browser, double x, double y)
{
}

void CefBrowserHandler::OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser,
                                                  const CefRange& selected_range,
                                                  const RectList& character_bounds)
{
}

void CefBrowserHandler::OnTextSelectionChanged(CefRefPtr<CefBrowser> browser, const CefString& selected_text, const CefRange& selected_range)
{
}

void CefBrowserHandler::OnVirtualKeyboardRequested(CefRefPtr<CefBrowser> browser, TextInputMode input_mode)
{
}

bool CefBrowserHandler::StartDragging(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDragData> drag_data, CefRenderHandler::DragOperationsMask allowed_ops, int x, int y)
{
    ASSERT(CefCurrentlyOn(TID_UI));
    if (!m_pHandleDelegate) {
        return false;
    }

    if (!m_dropTarget) {
        return false;
    }
    if ((browser == nullptr) || (browser->GetHost() == nullptr)) {
        return false;
    }

    m_currentDragOperation = DRAG_OPERATION_NONE;
    CefBrowserHost::DragOperationsMask result = m_dropTarget->StartDragging(this, drag_data, allowed_ops, x, y);
    m_currentDragOperation = DRAG_OPERATION_NONE;
    UiPoint pt = {};
    if ((m_pWindow != nullptr) && !m_windowFlag.expired()) {
        m_pWindow->GetCursorPos(pt);
        m_pWindow->ScreenToClient(pt);
    }
    m_pHandleDelegate->ClientToControl(pt);
    browser->GetHost()->DragSourceEndedAt(pt.x, pt.y, result);
    browser->GetHost()->DragSourceSystemDragEnded();
    return true;
}

void CefBrowserHandler::UpdateDragCursor(CefRefPtr<CefBrowser> browser, CefRenderHandler::DragOperation operation)
{
    ASSERT(CefCurrentlyOn(TID_UI));
    m_currentDragOperation = operation;
}

CefBrowserHost::DragOperationsMask CefBrowserHandler::OnDragEnter(CefRefPtr<CefDragData> drag_data, CefMouseEvent ev, CefBrowserHost::DragOperationsMask effect)
{
    ASSERT(CefCurrentlyOn(TID_UI));
    if ((m_browser != nullptr) && (m_browser->GetHost() != nullptr)) {
        UiPoint pt = { ev.x, ev.y };
        if (m_pHandleDelegate) {
            m_pHandleDelegate->ClientToControl(pt);
        }
        ev.x = pt.x;
        ev.y = pt.y;
        m_browser->GetHost()->DragTargetDragEnter(drag_data, ev, effect);
        m_browser->GetHost()->DragTargetDragOver(ev, effect);
    }
    return m_currentDragOperation;
}

CefBrowserHost::DragOperationsMask CefBrowserHandler::OnDragOver(CefMouseEvent ev, CefBrowserHost::DragOperationsMask effect)
{
    ASSERT(CefCurrentlyOn(TID_UI));
    if ((m_browser != nullptr) && (m_browser->GetHost() != nullptr)) {
        UiPoint pt = { ev.x, ev.y };
        if (m_pHandleDelegate) {
            m_pHandleDelegate->ClientToControl(pt);
        }
        ev.x = pt.x;
        ev.y = pt.y;
        m_browser->GetHost()->DragTargetDragOver(ev, effect);
    }
    return m_currentDragOperation;
}

void CefBrowserHandler::OnDragLeave()
{
    ASSERT(CefCurrentlyOn(TID_UI));
    if ((m_browser != nullptr) && (m_browser->GetHost() != nullptr)) {
        m_browser->GetHost()->DragTargetDragLeave();
    }
}

CefBrowserHost::DragOperationsMask CefBrowserHandler::OnDrop(CefMouseEvent ev, CefBrowserHost::DragOperationsMask effect)
{
    if ((m_browser != nullptr) && (m_browser->GetHost() != nullptr)) {
        UiPoint pt = { ev.x, ev.y };
        if (m_pHandleDelegate != nullptr) {
            m_pHandleDelegate->ClientToControl(pt);
        }
        ev.x = pt.x;
        ev.y = pt.y;
        m_browser->GetHost()->DragTargetDragOver(ev, effect);
        m_browser->GetHost()->DragTargetDrop(ev);
    }
    return m_currentDragOperation;
}

// CefContextMenuHandler methods
void CefBrowserHandler::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                            CefRefPtr<CefFrame> frame,
                                            CefRefPtr<CefContextMenuParams> params,
                                            CefRefPtr<CefMenuModel> model)
{
    ASSERT(CefCurrentlyOn(TID_UI));

    if (m_pHandleDelegate) {
        m_pHandleDelegate->OnBeforeContextMenu(browser, frame, params, model);
    }
    else  if ((params != nullptr) && (model != nullptr)) {
        // Customize the context menu...
        if ((params->GetTypeFlags() & (CM_TYPEFLAG_PAGE | CM_TYPEFLAG_FRAME)) != 0) {
            if (model->GetCount() > 0) {
                // 禁止右键菜单
                model->Clear();
            }
        }
    }
}

bool CefBrowserHandler::RunContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model, CefRefPtr<CefRunContextMenuCallback> callback)
{
    return false;
}

bool CefBrowserHandler::OnContextMenuCommand(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, int command_id, EventFlags event_flags)
{
    if (m_pHandleDelegate) {
        return m_pHandleDelegate->OnContextMenuCommand(browser, frame, params, command_id, event_flags);
    }
    return false;        
}

void CefBrowserHandler::OnContextMenuDismissed(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame)
{

}

bool CefBrowserHandler::RunQuickMenu(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     const CefPoint& location,
                                     const CefSize& size,
                                     QuickMenuEditStateFlags edit_state_flags,
                                     CefRefPtr<CefRunQuickMenuCallback> callback)
{
    return false;
}

bool CefBrowserHandler::OnQuickMenuCommand(CefRefPtr<CefBrowser> browser,
                                           CefRefPtr<CefFrame> frame,
                                           int command_id,
                                           EventFlags event_flags)
{
    return false;
}

void CefBrowserHandler::OnQuickMenuDismissed(CefRefPtr<CefBrowser> browser,
                                             CefRefPtr<CefFrame> frame)
{
}

// CefDisplayHandler methods
void CefBrowserHandler::OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& url)
{
    // Update the URL in the address bar...
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnAddressChange, m_pHandleDelegate, browser, frame, url));
    }
}

void CefBrowserHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
{
    // Update the browser window title...
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnTitleChange, m_pHandleDelegate, browser, title));
    }
}

void CefBrowserHandler::OnFaviconURLChange(CefRefPtr<CefBrowser> browser, const std::vector<CefString>& icon_urls)
{
}

void CefBrowserHandler::OnFullscreenModeChange(CefRefPtr<CefBrowser> browser, bool fullscreen)
{
}

bool CefBrowserHandler::OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text)
{
    return false;
}

void CefBrowserHandler::OnStatusMessage(CefRefPtr<CefBrowser> browser, const CefString& value)
{
}

bool CefBrowserHandler::OnConsoleMessage(CefRefPtr<CefBrowser> /*browser*/,
                                         cef_log_severity_t /*level*/,
                                         const CefString& /*message*/,
                                         const CefString& /*source*/,
                                         int /*line*/)
{
    // Log a console message...
    return true;
}

bool CefBrowserHandler::OnAutoResize(CefRefPtr<CefBrowser> browser, const CefSize& new_size)
{
    return false;
}

void CefBrowserHandler::OnLoadingProgressChange(CefRefPtr<CefBrowser> browser, double progress)
{
}

bool CefBrowserHandler::OnCursorChange(CefRefPtr<CefBrowser> browser,
                                       CefCursorHandle cursor,
                                       cef_cursor_type_t type,
                                       const CefCursorInfo& custom_cursor_info)
{
    if ((m_pWindow != nullptr) && !m_windowFlag.expired()) {
        SetClassLongPtr(m_pWindow->NativeWnd()->GetHWND(), GCLP_HCURSOR, static_cast<LONG>(reinterpret_cast<LONG_PTR>(cursor)));
    }
    SetCursor(cursor);
    return true;
}

void CefBrowserHandler::OnMediaAccessChange(CefRefPtr<CefBrowser> browser,
                                            bool has_video_access,
                                            bool has_audio_access) {
}

bool CefBrowserHandler::OnDragEnter(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDragData> dragData, CefDragHandler::DragOperationsMask mask)
{
    return false;
}

void CefBrowserHandler::OnDraggableRegionsChanged(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const std::vector<CefDraggableRegion>& regions)
{
}

// CefLoadHandler methods
void CefBrowserHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward)
{
    // Update UI for browser state...
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnLoadingStateChange, m_pHandleDelegate, browser, isLoading, canGoBack, canGoForward));
    }
}

void CefBrowserHandler::OnLoadStart(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    TransitionType transition_type)
{
    // A frame has started loading content...
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnLoadStart, m_pHandleDelegate, browser, frame, transition_type));
    }
}

void CefBrowserHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode)
{
    // A frame has finished loading content...
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnLoadEnd, m_pHandleDelegate, browser, frame, httpStatusCode));
    }
}

void CefBrowserHandler::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl)
{
    // A frame has failed to load content...
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnLoadError, m_pHandleDelegate, browser, frame, errorCode, errorText, failedUrl));
    }
}

bool CefBrowserHandler::OnJSDialog(CefRefPtr<CefBrowser> /*browser*/,
                                   const CefString& /*origin_url*/,
                                   JSDialogType /*dialog_type*/,
                                   const CefString& /*message_text*/,
                                   const CefString& /*default_prompt_text*/,
                                   CefRefPtr<CefJSDialogCallback> callback,
                                   bool& suppress_message)
{
    // release时阻止弹出js对话框
    (void)suppress_message;
#ifndef _DEBUG
    suppress_message = true;
#endif

    return false;
}

bool CefBrowserHandler::OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser,
                                             const CefString& message_text,
                                             bool is_reload,
                                             CefRefPtr<CefJSDialogCallback> callback)
{
    return false;
}

void CefBrowserHandler::OnResetDialogState(CefRefPtr<CefBrowser> browser)
{
}

void CefBrowserHandler::OnDialogClosed(CefRefPtr<CefBrowser> browser)
{
}

bool CefBrowserHandler::OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event, CefEventHandle os_event, bool* is_keyboard_shortcut)
{
    return false;
}

bool CefBrowserHandler::OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event, CefEventHandle os_event)
{
    return false;
}

// CefRequestHandler methods
bool CefBrowserHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       CefRefPtr<CefRequest> request,
                                       bool user_gesture,
                                       bool is_redirect)
{
    if (m_pHandleDelegate) {
        return m_pHandleDelegate->OnBeforeBrowse(browser, frame, request, user_gesture, is_redirect);
    }    
    return false;
}

bool CefBrowserHandler::OnOpenURLFromTab(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         const CefString& target_url,
                                         CefRequestHandler::WindowOpenDisposition target_disposition,
                                         bool user_gesture)
{
    return false;
}

CefRefPtr<CefResourceRequestHandler> CefBrowserHandler::GetResourceRequestHandler(CefRefPtr<CefBrowser> browser,
                                                                                  CefRefPtr<CefFrame> frame,
                                                                                  CefRefPtr<CefRequest> request,
                                                                                  bool is_navigation,
                                                                                  bool is_download,
                                                                                  const CefString& request_initiator,
                                                                                  bool& disable_default_handling)
{
    return this;
}

bool CefBrowserHandler::GetAuthCredentials(CefRefPtr<CefBrowser> browser,
                                           const CefString& origin_url,
                                           bool isProxy,
                                           const CefString& host,
                                           int port,
                                           const CefString& realm,
                                           const CefString& scheme,
                                           CefRefPtr<CefAuthCallback> callback)
{
    return false;
}

bool CefBrowserHandler::OnCertificateError(CefRefPtr<CefBrowser> browser,
                                           cef_errorcode_t cert_error,
                                           const CefString& request_url,
                                           CefRefPtr<CefSSLInfo> ssl_info,
                                           CefRefPtr<CefCallback> callback)
{
    return false;
}

bool CefBrowserHandler::OnSelectClientCertificate(CefRefPtr<CefBrowser> browser,
                                                  bool isProxy,
                                                  const CefString& host,
                                                  int port,
                                                  const X509CertificateList& certificates,
                                                  CefRefPtr<CefSelectClientCertificateCallback> callback)
{
    return false;
}

void CefBrowserHandler::OnRenderViewReady(CefRefPtr<CefBrowser> browser)
{
}

#if CEF_VERSION_MAJOR > 109
//CEF 高版本
bool CefBrowserHandler::OnRenderProcessUnresponsive(CefRefPtr<CefBrowser> browser,
                                                    CefRefPtr<CefUnresponsiveProcessCallback> callback)
{
    return false;
}

void CefBrowserHandler::OnRenderProcessResponsive(CefRefPtr<CefBrowser> browser)
{
}

void CefBrowserHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                                  TerminationStatus status,
                                                  int error_code,
                                                  const CefString& error_string)
{
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnRenderProcessTerminated, m_pHandleDelegate, browser, status, error_code, error_string));
    }
}
#else
void CefBrowserHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status)
{
    if (m_pHandleDelegate) {
        ui::GlobalManager::Instance().Thread().PostTask(ui::kThreadUI, UiBind(&CefBrowserHandlerDelegate::OnRenderProcessTerminated, m_pHandleDelegate, browser, status, 0, ""));
    }
}
#endif

void CefBrowserHandler::OnDocumentAvailableInMainFrame(CefRefPtr<CefBrowser> browser)
{
}

CefRefPtr<CefCookieAccessFilter> CefBrowserHandler::GetCookieAccessFilter(CefRefPtr<CefBrowser> browser,
                                                                          CefRefPtr<CefFrame> frame,
                                                                          CefRefPtr<CefRequest> request)
{
    return this;
}

CefResourceRequestHandler::ReturnValue CefBrowserHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                                                               CefRefPtr<CefFrame> frame,
                                                                               CefRefPtr<CefRequest> request,
                                                                               CefRefPtr<CefCallback> callback)
{
    if (m_pHandleDelegate) {
        return m_pHandleDelegate->OnBeforeResourceLoad(browser, frame, request, callback);
    }
    return RV_CONTINUE;
}

CefRefPtr<CefResourceHandler> CefBrowserHandler::GetResourceHandler(CefRefPtr<CefBrowser> browser,
                                                                    CefRefPtr<CefFrame> frame,
                                                                    CefRefPtr<CefRequest> request)
{
    return nullptr;
}

void CefBrowserHandler::OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                                           CefRefPtr<CefFrame> frame,
                                           CefRefPtr<CefRequest> request,
                                           CefRefPtr<CefResponse> response,
                                           CefString& new_url)
{
}

bool CefBrowserHandler::OnResourceResponse(CefRefPtr<CefBrowser> browser,
                                           CefRefPtr<CefFrame> frame,
                                           CefRefPtr<CefRequest> request,
                                           CefRefPtr<CefResponse> response)
{
    return false;
}

CefRefPtr<CefResponseFilter> CefBrowserHandler::GetResourceResponseFilter(CefRefPtr<CefBrowser> browser,
                                                                          CefRefPtr<CefFrame> frame,
                                                                          CefRefPtr<CefRequest> request,
                                                                          CefRefPtr<CefResponse> response)
{
    return nullptr;
}

void CefBrowserHandler::OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                                               CefRefPtr<CefFrame> frame,
                                               CefRefPtr<CefRequest> request,
                                               CefRefPtr<CefResponse> response,
                                               URLRequestStatus status,
                                               int64_t received_content_length)
{
}

void CefBrowserHandler::OnProtocolExecution(CefRefPtr<CefBrowser> browser, const CefString& url, bool& allow_os_execution)
{
    if (m_pHandleDelegate) {
        m_pHandleDelegate->OnProtocolExecution(browser, url, allow_os_execution);
    }
}

bool CefBrowserHandler::CanSendCookie(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefRequest> request,
                                      const CefCookie& cookie)
{
    return true;
}

bool CefBrowserHandler::CanSaveCookie(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefRequest> request,
                                      CefRefPtr<CefResponse> response,
                                      const CefCookie& cookie)
{
    return true;
}

bool CefBrowserHandler::CanDownload(CefRefPtr<CefBrowser> browser,
                                    const CefString& url,
                                    const CefString& request_method)
{
    return true;
}

#if CEF_VERSION_MAJOR <= 109
//CEF 109版本
void CefBrowserHandler::OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefDownloadItem> download_item,
                                         const CefString& suggested_name,
                                         CefRefPtr<CefBeforeDownloadCallback> callback)
{
    if (m_pHandleDelegate) {
        m_pHandleDelegate->OnBeforeDownload(browser, download_item, suggested_name, callback);
    }
}
#else
//CEF 高版本
bool CefBrowserHandler::OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefDownloadItem> download_item,
                                         const CefString& suggested_name,
                                         CefRefPtr<CefBeforeDownloadCallback> callback)
{
    if (m_pHandleDelegate) {
        return m_pHandleDelegate->OnBeforeDownload(browser, download_item, suggested_name, callback);
    }
    return true;
}
#endif

void CefBrowserHandler::OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefDownloadItem> download_item,
                                          CefRefPtr<CefDownloadItemCallback> callback)
{
    if (m_pHandleDelegate) {
        m_pHandleDelegate->OnDownloadUpdated(browser, download_item, callback);
    }
}

#if CEF_VERSION_MAJOR <= 109
//CEF 109版本
bool CefBrowserHandler::OnFileDialog(CefRefPtr<CefBrowser> browser,
                                     FileDialogMode mode,
                                     const CefString& title,
                                     const CefString& default_file_path,
                                     const std::vector<CefString>& accept_filters,
                                     CefRefPtr<CefFileDialogCallback> callback)
{
    if (m_pHandleDelegate) {
        return m_pHandleDelegate->OnFileDialog(browser, mode, title, default_file_path, accept_filters, std::vector<CefString>(), std::vector<CefString>(), callback);
    }
    return false;
}
#else
//CEF 高版本
bool CefBrowserHandler::OnFileDialog(CefRefPtr<CefBrowser> browser,
                                     FileDialogMode mode,
                                     const CefString& title,
                                     const CefString& default_file_path,
                                     const std::vector<CefString>& accept_filters,
                                     const std::vector<CefString>& accept_extensions,
                                     const std::vector<CefString>& accept_descriptions,
                                     CefRefPtr<CefFileDialogCallback> callback)
{
    if (m_pHandleDelegate) {
        return m_pHandleDelegate->OnFileDialog(browser, mode, title, default_file_path, accept_filters, accept_extensions, accept_descriptions, callback);
    }
    return false;
}
#endif
}

#pragma warning (pop)
