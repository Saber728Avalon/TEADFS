#include <iostream>
#include "pevents.h"
#include <list>
#include <memory>
#include <mutex>
#include <thread>

namespace TEAD {


#define TIMEOUT_INTERVAL 5000

#define MAX_REPEAT_COUNT 12

	enum THREAD_STATUS {
		//wait task to run
		TS_WAIT,
		//task is running
		TS_RUNNING,
		//thread exit.
		TS_EXIT,

		TS_COUNT
	};

	template <class T>
	class CTaskList {
	public:

		void Push(std::shared_ptr<T> ptr);

		std::shared_ptr<T> Pop();

	private:
		//mutex
		std::mutex m_mutex;
		//list
		std::list<std::shared_ptr<T>> m_list;
	};




	template<class T>
	class CThreadWork {
	public:
		typedef std::function<void(std::shared_ptr<T>)> handler;

		CThreadWork(CTaskList<T>& taskList, handler handle);
		virtual ~CThreadWork();

		//thread run
		void Runnable();

		//close and exit thread
		void Exit();

		//
		THREAD_STATUS Status();

		void Wakeup();
	private:
		//event
		neosmart::neosmart_event_t m_event;
		//exit thread
		bool m_bExit;
		//status
		THREAD_STATUS m_status;
		//
		std::shared_ptr<std::thread> m_ptrThread;

		CTaskList<T>& m_taskList;

		handler m_handler;
	};

	template<class T>
	class CThreadPool {
		public:
			typedef std::function<void(std::shared_ptr<T>)> handler;

			CThreadPool(handler handle);
			virtual ~CThreadPool();

			//add task to thread pool
			void AddTask(std::shared_ptr<T> ptr);

			//thread run main
			void Runnable();

		private:
			//list
			CTaskList<T> m_task;
			//
			bool m_bExit;
			//thread
			std::list<std::shared_ptr<CThreadWork<T>>> m_threadPool;
			// deal thread callback funcion
			handler m_handler;
			//
			std::shared_ptr<std::thread> m_ptrThread;
			//event
			neosmart::neosmart_event_t m_event;
		};


	/////////////////////////////////////////////////////////////TaskList///////////////////////////////////////////////////////////////////////////////
	template<class T>
	void CTaskList<T>::Push(std::shared_ptr<T> ptr) {
		std::lock_guard<std::mutex> lock(m_mutex);

		m_list.push_back(ptr);
		return;
	}

	template<class T>
	std::shared_ptr<T> CTaskList<T>::Pop() {
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_list.empty()) {
			return nullptr;
		}
		auto ptr = m_list.front();
		m_list.pop_front();
		return ptr;
	}
	/////////////////////////////////////////////////////////////CThreadWork///////////////////////////////////////////////////////////////////////////////
	template<class T>
	CThreadWork<T>::CThreadWork(CTaskList<T>& taskList, handler handle) :
		m_taskList(taskList)
		, m_handler(handle)
	{
		m_bExit = false;
		m_status = TS_WAIT;
		//create event
		m_event = neosmart::CreateEvent();
		//create thread
		m_ptrThread = std::make_shared<std::thread>([&]() {
			Runnable();
			});
		m_ptrThread->detach();
	}

	template<class T>
	CThreadWork<T>::~CThreadWork() {
		m_bExit = true;
		//release event
		DestroyEvent(m_event);
	}

	//thread run
	template<class T>
	void CThreadWork<T>::Runnable() {
		int nRepeatCount = MAX_REPEAT_COUNT;

		while (!m_bExit) {
			m_status = TS_RUNNING;

			auto ptrTask = m_taskList.Pop();
			if (nullptr == ptrTask) {
				// mark thread is wait
				m_status = TS_WAIT;
				//release cpu to other thread
				int nTimeout = neosmart::WaitForEvent(m_event, TIMEOUT_INTERVAL);
				if (WAIT_TIMEOUT == nTimeout) {
					nRepeatCount--;
					//max time not get task. release thread
					if (!nRepeatCount) {
						m_bExit = true;
					}
					continue;
				}
			}
			m_handler(ptrTask);
			//reset repeat times
			nRepeatCount = MAX_REPEAT_COUNT;
			//
		}
		m_status = TS_EXIT;
		return;
	}

	//close and exit thread
	template<class T>
	void CThreadWork<T>::Exit() {
		m_bExit = true;
	}

	template<class T>
	THREAD_STATUS CThreadWork<T>::Status() {
		return m_status;
	}


	template<class T>
	void CThreadWork<T>::Wakeup() {
		neosmart::SetEvent(m_event);
	}
	/////////////////////////////////////////////////////////////CThreadPool///////////////////////////////////////////////////////////////////////////////
	template<class T>
	CThreadPool<T>::CThreadPool(handler handle) :
		m_handler(handle)
	{
		m_bExit = false;
		//create event
		m_event = neosmart::CreateEvent();
		//create thread
		m_ptrThread = std::make_shared<std::thread>([&]() {
			Runnable();
		});
		m_ptrThread->detach();
	}

	template<class T>
	CThreadPool<T>::~CThreadPool() {
		m_bExit = true;
		//release event
		DestroyEvent(m_event);
	}

	template<class T>
	void CThreadPool<T>::AddTask(std::shared_ptr<T> ptr) {
		m_task.Push(ptr);
		neosmart::SetEvent(m_event);
	}



	//thread run
	template<class T>
	void  CThreadPool<T>::Runnable() {
		while (!m_bExit) {
			//release cpu to other thread
			int nTimeout = neosmart::WaitForEvent(m_event, TIMEOUT_INTERVAL);
			if (WAIT_TIMEOUT == nTimeout) {
				continue;
			}
			//find thread
			std::shared_ptr<CThreadWork<T>> ptrThread;
			for (auto iter = m_threadPool.begin(); iter != m_threadPool.end(); ) {
				if (TS_WAIT == (*iter)->Status()) {
					ptrThread = *iter;
				}
				else if (TS_EXIT == (*iter)->Status()) {
					iter = m_threadPool.erase(iter);
					continue;
				}
				iter++;
			}
			//not found, create new thread
			if (nullptr == ptrThread) {
				ptrThread = std::make_shared<CThreadWork<T>>(m_task, m_handler);
				m_threadPool.emplace_back(ptrThread);
			}
			//weke up thead
			ptrThread->Wakeup();
		}
		return;
	}
}