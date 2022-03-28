#include <iostream>
#include <windows.h>
#include <string.h>
#include <vector>
#include <algorithm>

struct Params {
    int* array;
    int thread_id;
    int array_size;
    HANDLE hEndWorkEvent; //individual
    HANDLE hCannotWorkEvent; //individual
    HANDLE hContinueEvent; //individual
    CRITICAL_SECTION* cs;
};

DWORD WINAPI marker(LPVOID param) {
    Params* parametr = (Params*)param;
    srand(parametr->thread_id);

    while (true) 
    {
        int value = rand() % parametr->array_size;
        EnterCriticalSection(*&parametr->cs);
        if (parametr->array[value] == 0) {
            Sleep(5);
            parametr->array[value] = parametr->thread_id;
            Sleep(5);
            LeaveCriticalSection(*&parametr->cs);
            continue;
        }
        else 
        {
            int marked = 0;
            int markedByMe = 0;
            for (size_t i = 0; i < parametr->array_size; i++)
            {
                if (parametr->array[i] != 0) {
                    marked++;
                    if (parametr->array[i] == parametr->thread_id) {
                        markedByMe++;
                    }
                }
            }
            std::cout << "\n\nMy number: #" << parametr->thread_id 
                << "\nMarked : " << marked 
                << "\nMarked by me : " << markedByMe
                << "\nCannot mark index: " << value << "\n";
            LeaveCriticalSection(*&parametr->cs);


            HANDLE hEvents[2] = { parametr->hContinueEvent, parametr->hEndWorkEvent };
            SetEvent(parametr->hCannotWorkEvent);
            if (WaitForMultipleObjects(2, hEvents, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
            {
                //was asked to continue
                continue;
            }
            else 
            {
                //was asked to stop
                std::cout << "\nI am #" << parametr->thread_id << " and i am erasing!\n";
                EnterCriticalSection(*&parametr->cs);
                for (size_t i = 0; i < parametr->array_size; i++)
                {
                    if (parametr->array[i] == parametr->thread_id) {
                        parametr->array[i] = 0;
                    }
                }
                LeaveCriticalSection(*&parametr->cs);
                return 0;
            }
        }
    }
   
    return 0;
}

void PrintArray(int* array, int length) {
    std::cout << "\n";
    for (size_t i = 0; i < length; i++)
    {
        std::cout << array[i] << " ";
    }
    std::cout << "\n";
}

int main() 
{
	std::cout << "Enter number of elements:\n";

	int size = 0;
	std::cin >> size;

    if (size <= 0 ) {
        std::cout << "Invalid array size.\n";
        return -1;
    }

	int* array = new int[size] {};

    std::cout << "Enter number of threads:\n";
    int threads_number = 0;
    std::cin >> threads_number;

    if (threads_number <= 0 || threads_number > 20)
    {
        std::cout << "Invalid array size.\n";
        return -1;
    }

    HANDLE* markers = new HANDLE[threads_number];

    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    HANDLE* handlesCannotContinue = new HANDLE[threads_number];
    std::vector<HANDLE> handlesContinueWork;
    HANDLE* handlesEndWork = new HANDLE[threads_number];

    for (size_t i = 0; i < threads_number; i++)
    {
        HANDLE cannotContinueWorkEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (cannotContinueWorkEvent == NULL) {
            std::cout << "Invalid handle. Failed to create event";
            return -1;
        }

        HANDLE continueWorkEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (continueWorkEvent == NULL) {
            std::cout << "Invalid handle. Failed to create event";
            return -1;
        }
        HANDLE endWorkEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (endWorkEvent == NULL) {
            std::cout << "Invalid handle. Failed to create event";
            return -1;
        }

        handlesCannotContinue[i] = cannotContinueWorkEvent;
        handlesContinueWork.push_back(continueWorkEvent);
        handlesEndWork[i] = endWorkEvent;

        Params* parametr = new Params;
        parametr->array = array;
        parametr->array_size = size;
        parametr->thread_id = i + 1;
        parametr->cs = &cs;
        parametr->hCannotWorkEvent = cannotContinueWorkEvent;
        parametr->hContinueEvent = continueWorkEvent;
        parametr->hEndWorkEvent = endWorkEvent;

        EnterCriticalSection(&cs);
        
        markers[i] = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size  
            marker,       // thread function name
            (Params*)parametr,          // argument to thread function 
            0,                      // use default creation flags 
            NULL);   // returns the thread identifier 
        if (NULL == markers[i]) 
        {
            std::cout << "Invalid handle. Failed to create thread";
            return -1;
        }
        LeaveCriticalSection(&cs);
    }

    //track alive threads
    int alive_threads = threads_number;
    bool* isAlive = new bool[threads_number];
    for (size_t i = 0; i < threads_number; i++)
    {
        isAlive[i] = true;
    }
    int thread_amount = threads_number;


    while (true) 
    {
        DWORD dwWaitResult = WaitForMultipleObjects(threads_number, handlesCannotContinue, TRUE, INFINITE);

        PrintArray(array, size);

        std::cout << "What thread to stop? \n";

        int user_id = 0;
        std::cin >> user_id;
        user_id--;

        if (user_id > thread_amount || user_id < 0 || isAlive[user_id] == false)
        {
            std::cout << "Invalid thread id.\n";
            break;
        }

        int id = 0;
        for (size_t i = 0; i < user_id; i++)
        {
            if (isAlive[i] == true) {
                id++;
            }
        }
        isAlive[user_id] = false;

        SetEvent(handlesEndWork[user_id]);
        WaitForSingleObject(markers[user_id], INFINITE);

        threads_number--;
        HANDLE* tempArr = new HANDLE[threads_number];

        CloseHandle(handlesCannotContinue[id]);
        memmove(tempArr, handlesCannotContinue, id * sizeof(HANDLE));
        memmove(&tempArr[id], &handlesCannotContinue[id + 1], (threads_number - id) * sizeof(HANDLE));

        handlesCannotContinue = tempArr;

        std::cout << "Array after stop:";

        PrintArray(array, size);

        if (threads_number == 0) {
            std::cout << "No threads left more. End of job...\n";
            break;
        }

        CloseHandle(handlesContinueWork[id]);
        remove(handlesContinueWork.begin(), handlesContinueWork.end(), handlesContinueWork[id]);
        for (size_t i = 0; i < threads_number; i++)
        {
            SetEvent(handlesContinueWork[i]);
        }
    }

    for (size_t i = 0; i < thread_amount; i++)
    {
        CloseHandle(markers[i]);

        CloseHandle(handlesEndWork[i]);
        if (i < threads_number) {
            CloseHandle(handlesContinueWork[i]);
            CloseHandle(handlesCannotContinue[i]);
        }
    }

    delete[] markers;
    delete[] handlesEndWork;
    delete[] array;
    delete[] isAlive;

    return 0;
}