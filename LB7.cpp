// LB7777.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <windows.h>
#include <iostream>
#include <string>

#define BUFFER_SIZE 1000
#define CONSTANT_TIMEOUT 1000
#define STR_BUFFER_SIZE 20

using namespace std;

void Server(char* path);
void Client();

int main(int argc, char* argv[])
{
	switch (argc)
	{
	case 1:
		Server(argv[0]);
		break;
	default:
		Client();
		break;
	}
}

void Server(char* path)
{
	string portName = "COM1";

	HANDLE hPortServer;
	HANDLE SemaphoreSet[3];

	string write_str;
	char buffer[STR_BUFFER_SIZE];

	SemaphoreSet[0] = CreateSemaphore(
		NULL,// атрибут доступа
		0,// инициализированное начальное состояние счетчика
		1,// максимальное количество обращений
		"READ"// имя объекта
	);
	SemaphoreSet[1] = CreateSemaphore(NULL, 0, 1, "WRITE");
	SemaphoreSet[2] = CreateSemaphore(NULL, 0, 1, "EXIT");

	cout << "COM1 port:SERVER\n";

	hPortServer = CreateFile(
		(LPSTR)portName.c_str(),//имя порта
		GENERIC_READ | GENERIC_WRITE,//для записи и чтения
		0,//совместный доступ
		NULL,//атрибуты защиты
		OPEN_EXISTING,//открыть существующий
		FILE_ATTRIBUTE_NORMAL,
		NULL//дескриптор шаблона файла
	);

	if (hPortServer == INVALID_HANDLE_VALUE) {
		cout << "Error during opening COM1" << endl;
		return;
	}

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcess(
		(LPSTR)path,//путь для создания нового процесса
		(LPSTR)" Client",//имя процесса
		NULL,// атрибуты защиты для нового процесса
		NULL,//атрибуты защиты для первичного потока
		FALSE,//флаг наследования
		CREATE_NEW_CONSOLE,//процесс создаётся в новой консоли 
		NULL,//переменные окружения
		NULL,//текущий диск и каталог.
		&si,
		&pi
	)) {
		CloseHandle(hPortServer);
		cout << "Error during creating client process" << endl;
		return;
	}

	SetCommMask(hPortServer, EV_RXCHAR);//определение отслеживаемых событий(символ был принят и помещён в буфер ввода данных)
	SetupComm(hPortServer, BUFFER_SIZE, BUFFER_SIZE);//коммуникационные пар-ры(размер буферов ввода/вывода)

	DCB dcb;//настройки управления
	memset(&dcb, 0, sizeof(dcb));//выделение памяти под dcb
	dcb.DCBlength = sizeof(DCB);//длина структуры в байтах
	if (!GetCommState(hPortServer, &dcb)) {
		CloseHandle(hPortServer);
		cout << "Error during DCB initialize" << endl;
		return;
	}

	dcb.BaudRate = CBR_14400;//скорость передачи данных в бодах
	dcb.fBinary = TRUE;//двоичный режим
	dcb.fParity = FALSE;//режим чётности
	dcb.fOutxCtsFlow = FALSE;// проверяется сигнал готовности к приему (CTS)
	dcb.fOutxDsrFlow = FALSE;// проверяется сигнал готовности модема (DSR)
	dcb.fDtrControl = DTR_CONTROL_DISABLE;//Сигнал  DTR (готовности терминала к передаче данных)
	dcb.fInX = FALSE;//используется ли XON/XOFF управление потоком данных в ходе приема
	dcb.fOutX = FALSE;//используется ли XON/XOFF управление потоком данных в ходе передачи	
	dcb.fNull = FALSE;// при приеме пустые байты не сбрасываются
	dcb.fRtsControl = RTS_CONTROL_DISABLE;//Сигнал RTS ( готовности к передаче) 
	dcb.fAbortOnError = TRUE;//драйвер завершает все операции чтения и записи с состоянием ошибки, если происходит ошибка
	dcb.XonLim = 255;//Минимальное число байтов, которое допустимо в буфере ввода данных
	dcb.XoffLim = 255;//Максимальное число байтов, допустимое в буфере ввода данных
	dcb.ByteSize = 8; //число информационных бит в передаваемых и принимаемых байтах.
	dcb.Parity = NOPARITY;//нет проверки чётности
	dcb.StopBits = ONESTOPBIT;//1 стоповый бит	
	dcb.XonChar = 0; // значение XON 
	dcb.XoffChar = 255;// величина XOFF

	if (!SetCommState(hPortServer, &dcb)) {//установка конфигурации
		CloseHandle(hPortServer);
		cout << "Error during setting configuration" << endl;
		return;
	}

	COMMTIMEOUTS timeouts;//структура для установки интервалов простоя
	timeouts.ReadIntervalTimeout = 0xFFFFFFFF;// Mаксимальный интервал простоя между поступлением двух символов по линии связи
	timeouts.ReadTotalTimeoutMultiplier = 0;//Множитель, используемый, чтобы вычислить полный период времени простоя для операций чтения
	timeouts.ReadTotalTimeoutConstant = CONSTANT_TIMEOUT;//Константа, используемая, чтобы вычислить полный период времени простоя для операций чтения
	timeouts.WriteTotalTimeoutMultiplier = 0;// Множитель, используемый, чтобы вычислить полный период времени простоя для операций записи
	timeouts.WriteTotalTimeoutConstant = CONSTANT_TIMEOUT;// Константа, используемая, чтобы вычислить полный период времени простоя для операций записи

	if (!SetCommTimeouts(hPortServer, &timeouts)) {//установка интервалов простоя
		CloseHandle(hPortServer);
		cout << "Error during setting timeouts" << endl;
		return;
	}
	cout << "Enter 'e' for exit\n";
	while (true)
	{
		DWORD writtenBytes = 0;

		cout << "Enter string to pass: ";
		cin.clear();
		getline(cin, write_str);

		if (write_str == "e") {
			ReleaseSemaphore(SemaphoreSet[2], 1, NULL);
			WaitForSingleObject(pi.hProcess, INFINITE);
			break;
		}

		ReleaseSemaphore(SemaphoreSet[0], 1, NULL);//освобождение счётчика

		int portionNumber = write_str.size() / STR_BUFFER_SIZE + 1;//столько раз будут передаваться данные из буфера
		//отправляем 2 порту
		if (!WriteFile(hPortServer, &portionNumber, sizeof(portionNumber), &writtenBytes, NULL)) {
			cout << "Error during writing number of data portions" << endl;
			break;
		}

		int size = write_str.size();
		//отправляем 2 порту размер введённой строки
		if (!WriteFile(hPortServer, &size, sizeof(size), &writtenBytes, NULL)) {
			cout << "Error during writing entered string size" << endl;
			break;
		}

		for (int i = 0; i < portionNumber; i++) {
			write_str.copy(buffer, STR_BUFFER_SIZE, i * STR_BUFFER_SIZE);//копируем в буфер части введённой строки
			if (!WriteFile(hPortServer, buffer, STR_BUFFER_SIZE, &writtenBytes, NULL)) {
				cout << "Error during writing in COM1";
				CloseHandle(hPortServer);
				CloseHandle(SemaphoreSet[0]);
				CloseHandle(SemaphoreSet[1]);
			}
		}

		WaitForSingleObject(SemaphoreSet[1], INFINITE);//ждём сигнала от ридера
	}

	CloseHandle(hPortServer);
	CloseHandle(SemaphoreSet[0]);
	CloseHandle(SemaphoreSet[1]);
	CloseHandle(SemaphoreSet[2]);

	return;
}




void Client()
{
	string portName = "COM2";

	HANDLE hPortClient;
	HANDLE SemaphoreSet[3];

	string read_str;
	char buffer[STR_BUFFER_SIZE];

	SemaphoreSet[0] = OpenSemaphore(SEMAPHORE_ALL_ACCESS, TRUE, "READ");
	SemaphoreSet[1] = OpenSemaphore(SEMAPHORE_ALL_ACCESS, TRUE, "WRITE");
	SemaphoreSet[2] = OpenSemaphore(SEMAPHORE_ALL_ACCESS, TRUE, "EXIT");

	cout << "COM2\n";

	hPortClient = CreateFile(
		(LPSTR)portName.c_str(),//имя порта
		GENERIC_READ | GENERIC_WRITE,//для записи и чтения
		0,//совместный доступ
		NULL,//атрибуты защиты
		OPEN_EXISTING,//открыть существующий
		FILE_ATTRIBUTE_NORMAL,
		NULL//дескриптор шаблона файла
	);

	while (true)
	{
		DWORD readBytes;
		read_str.clear();

		int semaphoreIndex = WaitForMultipleObjects(3, SemaphoreSet, FALSE, INFINITE);
		if (semaphoreIndex - WAIT_OBJECT_0 == 2)//пользователь ввёл 'q'
		{
			break;
		}

		int portionNumber;
		if (!ReadFile(hPortClient, &portionNumber, sizeof(portionNumber), &readBytes, NULL)) {//считываем количество порций для чтения
			cout << "Error during reading number of data portions" << endl;
			break;
		}

		int size;
		if (!ReadFile(hPortClient, &size, sizeof(size), &readBytes, NULL)) {//считываем размер введённой строки
			cout << "Error during reading entered string size" << endl;
			break;
		}

		for (int i = 0; i < portionNumber; i++)
		{
			if (!ReadFile(hPortClient, buffer, STR_BUFFER_SIZE, &readBytes, NULL)) {//считываем в буфер часть введённой строки
				cout << "Error during reading from COM2" << endl;
				CloseHandle(hPortClient);
				CloseHandle(SemaphoreSet[0]);
				CloseHandle(SemaphoreSet[1]);
				return;
			}

			read_str.append(buffer, STR_BUFFER_SIZE);//расширяет строку
		}


		read_str.resize(size);//действительный размер стоки

		cout << "COM2: " << endl;
		for (int i = 0; i < size; i++)
		{
			cout << read_str[i];
		}
		cout << endl;

		ReleaseSemaphore(SemaphoreSet[1], 1, NULL);//освобождение счётчика, чтение завершено
	}

	CloseHandle(hPortClient);
	CloseHandle(SemaphoreSet[0]);
	CloseHandle(SemaphoreSet[1]);
	CloseHandle(SemaphoreSet[2]);
	return;
}
