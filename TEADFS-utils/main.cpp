#include <iostream>
#include "thread_pool.h"


void callback_func(std::shared_ptr<std::string> ptr) {
	printf("dddd\n");
	return;
}

std::shared_ptr<TEAD::CThreadPool<std::string>> g_ptrThreadPool;

int StartTEADFS() {

	g_ptrThreadPool = std::make_shared<TEAD::CThreadPool<std::string>>(callback_func);
	g_ptrThreadPool->AddTask(std::make_shared<std::string>());
	return 1;
}