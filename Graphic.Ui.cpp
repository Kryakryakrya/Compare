// Graphic.Ui.cpp : Определяет точку входа для приложения.
//

#include "framework.h"
#include "Graphic.Ui.h"
#include <strsafe.h>

UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;
typedef std::basic_string<TCHAR> Tstring;
#define MAX_LOADSTRING 100
std::deque<Tstring> commandDeque; // Вектор команд
std::mutex commandMutex; // Мьютекс для защиты доступа к вектору команд
std::condition_variable commandCondition; // Условная переменная для ожидания новых команд
OPENFILENAME ofn;
TCHAR szFile[MAX_PATH] = { 0 };
LPTSTR pathFolder;
BROWSEINFO bi = { 0 };
LPITEMIDLIST pidl;
// {80C59810-7034-41C9-A9B0-EBE65B98F52B}
static const GUID iconGuid =
{ 0x80c59810, 0x7034, 0x41c9, { 0xa9, 0xb0, 0xeb, 0xe6, 0x5b, 0x98, 0xf5, 0x2b } };



// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL AddNotificationIcon(HWND hwnd);
BOOL DeleteNotificationIcon();
void ShowContextMenu(HWND hwnd, POINT pt);
BOOL Read(HANDLE handle, uint8_t* data, uint64_t length, DWORD& bytesRead);
BOOL Write(HANDLE handle, uint8_t* data, uint64_t length);
HANDLE ConnectToServerPipe(const Tstring& name, uint32_t timeout);
Tstring GetErrorAsString(DWORD errorMessegeID);
SECURITY_ATTRIBUTES GetSecurityAttributes(const Tstring& sddl);
Tstring GetCurrentUserSid();
Tstring GetUserSid(HANDLE userToken);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    // Убираем предупрждение, тк не используем эти переменные
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);


    int nArgs;
    LPTSTR* arglist = CommandLineToArgvW(GetCommandLine(), &nArgs);

    if (NULL == arglist)
        return -1;

    if (nArgs > 1)
    {
        if (arglist[1] == Tstring(__TEXT("--secure-desktop")))
        {
            HDESK hCurrentDesktop = OpenInputDesktop(
                0,
                TRUE,
                DESKTOP_SWITCHDESKTOP
            );

            auto sddl = std::format(__TEXT("O:{0}G:{0}D:"), GetCurrentUserSid());
            auto security = GetSecurityAttributes(sddl);

            HDESK hDesk = CreateDesktop(
                __TEXT("AntimalwareCD"),
                NULL,
                NULL,
                0,
                DESKTOP_CREATEWINDOW | DESKTOP_SWITCHDESKTOP,
                &security
            );

            SetThreadDesktop(hDesk);

            if (!SwitchDesktop(hDesk))
                return -1;

            bool result = (IDYES == MessageBox(
                NULL,
                __TEXT("Are you shure?"),
                __TEXT("Warning"),
                MB_ICONINFORMATION | MB_YESNO));

            SwitchDesktop(hCurrentDesktop);
            SetThreadDesktop(hCurrentDesktop);

            CloseDesktop(hDesk);
            CloseDesktop(hCurrentDesktop);
            return result ? 1 : 0;
        }
    }

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_GRAPHICUI, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Выполнить инициализацию приложения:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GRAPHICUI));

    MSG msg;

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GRAPHICUI));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_GRAPHICUI);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
//   КОММЕНТАРИИ:
//
//        В этой функции маркер экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   /*ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);*/
   AddNotificationIcon(hWnd);
   std::thread clientThread([hWnd]()
       {
           DWORD sessionId;
           ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);

           Tstring path{ std::format(__TEXT("\\\\.\\pipe\\AntimalwareServiceIPC\\{}"), sessionId) };
           HANDLE pipe = ConnectToServerPipe(path, NMPWAIT_WAIT_FOREVER);
           DWORD length = 0;
           Tstring command;
           while (true) {
               // Ждем новой команды
               {
                   std::unique_lock<std::mutex> lock(commandMutex);
                   commandCondition.wait(lock, []
                       {
                           return !commandDeque.empty();
                       });
                   // Получаем команду из очереди
                   command = commandDeque.front();
                   commandDeque.pop_front();
               }

               // Отправляем команду сервису
               Write(pipe, reinterpret_cast<uint8_t*>(command.data()), 512);

               uint8_t  buff2[6] = {};
               Read(pipe, buff2, 6, length);
              
               // Отображаем результат сканирования в виде алерта
               if (buff2[0] == 'V') {
                   MessageBox(hWnd, __TEXT("Обнаружен вирус!"), __TEXT("Результат сканирования"), MB_OK | MB_ICONERROR);
               }
               else if (buff2[0] == 'N') {
                   MessageBox(hWnd, __TEXT("Не обнаружен вирус."), __TEXT("Результат сканирования"), MB_OK | MB_ICONINFORMATION);
               }
               else if (buff2[0] == 'D') {
                   MessageBox(hWnd, TEXT("Выбранный путь является папкой."), TEXT("Результат сканирования"), MB_OK | MB_ICONINFORMATION);
               }
               else {
                   MessageBox(hWnd, __TEXT("Ошибка при сканировании."), __TEXT("Результат сканирования"), MB_OK | MB_ICONWARNING);
               }
           }

           CloseHandle(pipe);
       });
   clientThread.detach();

   return TRUE;
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
//
//  WM_COMMAND  - обработать меню приложения
//  WM_PAINT    - Отрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Разобрать выбор в меню:
            switch (wmId)
            {
            case ID_SHOW_MAIN_WINDOW:
                ShowWindow(hWnd, SW_SHOW);
                UpdateWindow(hWnd);
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case ID_EXIT:
            case IDM_EXIT:
                // Помещаем команду на выход в вектор команд
                // Добавляем команду в очередь
            {
                std::lock_guard<std::mutex> lock(commandMutex);
                commandDeque.push_front(__TEXT("Q"));
            }
            // Оповещаем ожидающий поток о наличии новой команды
            commandCondition.notify_one();
            break;
            case IDM_SCAN:
                // Структура для настройки диалога выбора файла

                // Инициализация структуры
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFile = szFile;
                ofn.lpstrFile[0] = '\0';
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFilter = __TEXT("All Files\0*.*\0");
                ofn.nFilterIndex = 1;
                ofn.lpstrInitialDir = NULL;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                // Открытие диалога выбора файла
                if (GetOpenFileName(&ofn) == TRUE)
                {
                    // Получение выбранного пути к файлу в szFile

                    Tstring filePath(szFile);

                    // Помещаем путь на выход в вектор команд
                    // Добавляем команду в очередь
                    {
                        std::lock_guard<std::mutex> lock(commandMutex);
                        commandDeque.push_front(filePath);
                    }
                    // Оповещаем ожидающий поток о наличии новой команды
                    commandCondition.notify_one();
                }
                break;
            case IDM_SCAN_FOLDER:
                // Структура для диалога выбора папки
                IFileDialog* pfd;
                if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd))))
                {
                    DWORD dwOptions;
                    if (SUCCEEDED(pfd->GetOptions(&dwOptions)))
                    {
                        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
                    }
                    if (SUCCEEDED(pfd->Show(NULL)))
                    {
                        IShellItem* psi;
                        if (SUCCEEDED(pfd->GetResult(&psi)))
                        {
                            if (!SUCCEEDED(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pathFolder)))
                            {
                                MessageBox(NULL, __TEXT("GetIDListName() failed"), NULL, NULL);
                            }
                            psi->Release();
                        }
                    }
                    pfd->Release();
                }

                // Помещаем путь на выход в вектор команд
                    // Добавляем команду в очередь
                {
                    std::lock_guard<std::mutex> lock(commandMutex);
                    commandDeque.push_front(pathFolder);
                }
                commandCondition.notify_one();
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Добавьте сюда любой код прорисовки, использующий HDC...
            EndPaint(hWnd, &ps);
        }
        break;
    case WMAPP_NOTIFYCALLBACK:
        switch (LOWORD(lParam))
        {
        case NIN_SELECT:
            ShowWindow(hWnd, SW_SHOW);
            UpdateWindow(hWnd);
            break;
        case WM_CONTEXTMENU:
            POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
            ShowContextMenu(hWnd, pt);
            break;
        }
        break;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

BOOL AddNotificationIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hwnd;
    // add the icon, setting the icon, tooltip, and callback message.
    // the icon will be identified with the GUID
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
    nid.guidItem = iconGuid;
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_GRAPHICUI));
    LoadString(hInst, IDS_TOOLTIP, nid.szTip, ARRAYSIZE(nid.szTip));
    Shell_NotifyIcon(NIM_ADD, &nid);

    // NOTIFYICON_VERSION_4 is prefered
    nid.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

BOOL DeleteNotificationIcon()
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_GUID;
    nid.guidItem = iconGuid;
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowContextMenu(HWND hwnd, POINT pt)
{
    HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU2));
    if (hMenu)
    {
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        if (hSubMenu)
        {
            // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
            SetForegroundWindow(hwnd);

            // respect menu drop alignment
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
            {
                uFlags |= TPM_RIGHTALIGN;
            }
            else
            {
                uFlags |= TPM_LEFTALIGN;
            }

            TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
        }
        DestroyMenu(hMenu);
    }
}

HANDLE ConnectToServerPipe(const Tstring& name, uint32_t timeout) {

    HANDLE hPipe = INVALID_HANDLE_VALUE;

    while (true) {
        hPipe = CreateFile(
            reinterpret_cast<LPCTSTR>(name.c_str()),
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (hPipe != INVALID_HANDLE_VALUE) break;

        if (GetLastError() != ERROR_PIPE_BUSY) {

            Tstring errorMessage = GetErrorAsString(GetLastError());
            MessageBox(NULL, errorMessage.c_str(), __TEXT("CreateFileEx error"), MB_ICONERROR | MB_OK);
            return INVALID_HANDLE_VALUE;
        }

        if (!WaitNamedPipe(reinterpret_cast<LPCTSTR>(name.c_str()), timeout)) {
            //("Could not open pipe: 20 second wait timed out. ");
            return INVALID_HANDLE_VALUE;
        }
    }
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    BOOL fSuccess = SetNamedPipeHandleState(
        hPipe,
        &dwMode,
        NULL,
        NULL
    );

    if (!fSuccess) {

        //("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError();
        return INVALID_HANDLE_VALUE;
    }

    return hPipe;
}


BOOL Write(HANDLE handle, uint8_t* data, uint64_t length) {

    DWORD cbWritten = 0;
    BOOL fSuccess = WriteFile(
        handle,
        data,
        length,
        &cbWritten,
        NULL
    );

    if (!fSuccess || length != cbWritten) return false;

    return true;
}

BOOL Read(HANDLE handle, uint8_t* data, uint64_t length, DWORD& bytesRead) {

    bytesRead = 0;
    BOOL fSuccess = ReadFile(
        handle,
        data,
        length,
        &bytesRead,
        NULL
    );

    if (!fSuccess || bytesRead == 0) return false;

    return true;
}

Tstring GetErrorAsString(DWORD errorMessegeID)
{
    if (errorMessegeID == 0) {
        return Tstring();
    }

    LPTSTR messegeBuffer = nullptr;

    size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorMessegeID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&messegeBuffer, 0, NULL);

    Tstring messege(messegeBuffer, size);

    LocalFree(messegeBuffer);

    return messege;
}

Tstring GetUserSid(HANDLE userToken) {

    Tstring userSid;
    DWORD err = 0;
    LPVOID pvInfo = NULL;
    DWORD cbSize = 0;

    if (!GetTokenInformation(userToken, TokenUser, NULL, 0, &cbSize))
    {
        err = GetLastError();
        if (err == ERROR_INSUFFICIENT_BUFFER)
        {
            err = 0;
            pvInfo = LocalAlloc(LPTR, cbSize);

            if (!pvInfo)
                err = ERROR_OUTOFMEMORY;
            else if (!GetTokenInformation(userToken, TokenUser, pvInfo, cbSize, &cbSize))
                err = GetLastError();
            else
            {
                err = 0;
                const TOKEN_USER* pUser = (const TOKEN_USER*)pvInfo;
                LPTSTR userSidBuf;
                ConvertSidToStringSid(pUser->User.Sid, &userSidBuf);
                userSid.assign(userSidBuf);
                LocalFree(userSidBuf);
            }
        }
    }

    return userSid;
}

Tstring GetCurrentUserSid() {

    HANDLE token;

    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &token))
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return __TEXT("");
    }
    return GetUserSid(token);
}

SECURITY_ATTRIBUTES GetSecurityAttributes(const Tstring& sddl) {

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    securityAttributes.bInheritHandle = TRUE;

    PSECURITY_DESCRIPTOR psd = nullptr;

    if (ConvertStringSecurityDescriptorToSecurityDescriptor(sddl.c_str(), SDDL_REVISION_1, &psd, nullptr))
        securityAttributes.lpSecurityDescriptor = psd;

    return securityAttributes;
}

