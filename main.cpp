#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <Windows.h>

// spend time without calling kernel sleep
void SpinWait(std::chrono::milliseconds dur) {
    auto start = std::chrono::system_clock::now();
    while (dur > std::chrono::system_clock::now() - start) {}
}

std::recursive_mutex mutex_;

LONG WINAPI
VectoredHandler(PEXCEPTION_POINTERS ex)
{
    {
        auto lock = std::unique_lock<std::recursive_mutex>(mutex_);
        SpinWait(std::chrono::milliseconds(1));
    }

    // Doing this does not seem to properly release the other lock:
    SuspendThread(GetCurrentThread());

    // Skip faulty instruction, may invoke the handler several times
    ex->ContextRecord->Rip++;

    return EXCEPTION_CONTINUE_EXECUTION;
}

void main_spin(const std::wstring& ident) {
    const auto name = L"main_spin_" + ident;
    SetThreadDescription(GetCurrentThread(), name.c_str());

    int i = 0;
    while (i < 10) {
        {
            auto lock = std::unique_lock<std::recursive_mutex>(mutex_);
            std::wcout << ident << L" spinning..  " << ++i << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void main_fail() {
    SetThreadDescription(GetCurrentThread(), L"main_fail");

    auto lock = std::unique_lock<std::recursive_mutex>(mutex_);

    std::cout << "I'm in danger..." << std::endl;
    char* ptr = 0;
    *ptr = 0;
    std::cout << ".. I survived" << std::endl;
}

int main()
{
    SetThreadDescription(GetCurrentThread(), L"main");

    // Install handler
    auto vech = AddVectoredExceptionHandler(1, VectoredHandler);

    std::thread t_spin_a([]() {main_spin(L"A"); });
    // std::thread t_spin_b([]() {main_spin(L"B"); });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::thread t_fail(main_fail);

    t_spin_a.join();
    // t_spin_b.join();
    t_fail.join();

    return 0;
}
