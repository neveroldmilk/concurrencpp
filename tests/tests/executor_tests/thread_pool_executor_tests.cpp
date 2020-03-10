#include "concurrencpp.h"
#include "executor_test_helpers.h"

#include "../all_tests.h"

#include "../../tester/tester.h"
#include "../../helpers/assertions.h"
#include "../../helpers/object_observer.h"

#include "../../concurrencpp/src/executors/constants.h"

namespace concurrencpp::tests {
	void test_thread_pool_executor_name();

	void test_thread_pool_executor_shutdown_coro_raii();
	void test_thread_pool_executor_shutdown_thread_join();
	void test_thread_pool_executor_shutdown_enqueue();
	void test_thread_pool_executor_shutdown();

	void test_thread_pool_executor_post();
	void test_thread_pool_executor_submit();

	void test_thread_pool_executor_bulk_post();
	void test_thread_pool_executor_bulk_submit();

	void test_thread_pool_executor_enqueue_algorithm();
	void test_thread_pool_executor_dynamic_resizing();
}

void concurrencpp::tests::test_thread_pool_executor_name() {
	const auto name = "abcde12345&*(";
	auto executor = std::make_shared<concurrencpp::thread_pool_executor>(name, 4, std::chrono::seconds(10));
	executor_shutdowner shutdowner(executor);
	assert_equal(executor->name, name);
}

void concurrencpp::tests::test_thread_pool_executor_shutdown_coro_raii() {
	object_observer observer;
	const size_t task_count = 1'024;
	auto executor = std::make_shared<thread_pool_executor>("threadpool", 1, std::chrono::seconds(4));

	std::vector<value_testing_stub> stubs;
	stubs.reserve(task_count);

	for (size_t i = 0; i < task_count; i++) {
		stubs.emplace_back(observer.get_testing_stub(i));
	}

	executor->post([] {
		std::this_thread::sleep_for(std::chrono::seconds(2));
	});

	auto results = executor->template bulk_submit<value_testing_stub>(stubs);

	executor->shutdown();
	assert_true(executor->shutdown_requested());

	assert_equal(observer.get_execution_count(), size_t(0));
	assert_equal(observer.get_destruction_count(), task_count);

	for (auto& result : results) {
		assert_throws<concurrencpp::errors::broken_task>([&result] {
			result.get();
		});
	}
}

void concurrencpp::tests::test_thread_pool_executor_shutdown_thread_join() {
	auto executor = std::make_shared<thread_pool_executor>("threadpool", 9, std::chrono::seconds(1));

	for (size_t i = 0; i < 3; i++) {
		executor->post([] {});
	}

	for (size_t i = 0; i < 3; i++) {
		executor->post([] {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		});
	}

	//allow threads time to start working
	std::this_thread::sleep_for(std::chrono::milliseconds(150));

	// 1/3 of the threads are waiting, 1/3 are working, 1/3 are idle. all should be joined when tp is shut-down.
	executor->shutdown();
	assert_true(executor->shutdown_requested());
}

void concurrencpp::tests::test_thread_pool_executor_shutdown_enqueue() {
	auto executor = std::make_shared<thread_pool_executor>("threadpool", 4, std::chrono::seconds(10));
	assert_false(executor->shutdown_requested());

	executor->shutdown();
	assert_true(executor->shutdown_requested());

	assert_throws<concurrencpp::errors::executor_shutdown>([executor] {
		executor->enqueue(std::experimental::coroutine_handle{});
	});

	assert_throws<concurrencpp::errors::executor_shutdown>([executor] {
		std::experimental::coroutine_handle<> array[4];
		std::span<std::experimental::coroutine_handle<>> span = array;
		executor->enqueue(span);
	});
}

void concurrencpp::tests::test_thread_pool_executor_shutdown() {
	test_thread_pool_executor_shutdown_coro_raii();
	test_thread_pool_executor_shutdown_thread_join();
	test_thread_pool_executor_shutdown_enqueue();
}

void concurrencpp::tests::test_thread_pool_executor_post() {
	object_observer observer;
	const size_t task_count = 100'000;
	const size_t worker_count = 6;
	auto executor = std::make_shared<thread_pool_executor>("threadpool", worker_count, std::chrono::seconds(10));
	executor_shutdowner shutdown(executor);

	for (size_t i = 0; i < task_count; i++) {
		executor->post(observer.get_testing_stub());
	}

	assert_true(observer.wait_execution_count(task_count, std::chrono::minutes(2)));
	assert_true(observer.wait_destruction_count(task_count, std::chrono::minutes(2)));

	assert_equal(observer.get_execution_map().size(), worker_count);
}

void concurrencpp::tests::test_thread_pool_executor_submit() {
	object_observer observer;
	const size_t task_count = 100'000;
	const size_t worker_count = 6;
	auto executor = std::make_shared<thread_pool_executor>("threadpool", worker_count, std::chrono::seconds(10));
	executor_shutdowner shutdown(executor);

	std::vector<result<size_t>> results;
	results.resize(task_count);

	for (size_t i = 0; i < task_count; i++) {
		results[i] = executor->submit(observer.get_testing_stub(i));
	}

	assert_true(observer.wait_execution_count(task_count, std::chrono::minutes(2)));
	assert_true(observer.wait_destruction_count(task_count, std::chrono::minutes(2)));

	assert_equal(observer.get_execution_map().size(), worker_count);

	for (size_t i = 0; i < task_count; i++) {
		assert_equal(results[i].get(),  i);
	}
}

void concurrencpp::tests::test_thread_pool_executor_bulk_post() {
	object_observer observer;
	const size_t task_count = 40'000;
	const size_t worker_count = 6;
	auto executor = std::make_shared<thread_pool_executor>("threadpool", worker_count, std::chrono::seconds(10));
	executor_shutdowner shutdown(executor);

	std::vector<testing_stub> stubs;
	stubs.reserve(task_count);

	for (size_t i = 0; i < task_count; i++) {
		stubs.emplace_back(observer.get_testing_stub());
	}

	executor->template bulk_post<testing_stub>(stubs);

	assert_true(observer.wait_execution_count(task_count, std::chrono::minutes(2)));
	assert_true(observer.wait_destruction_count(task_count, std::chrono::minutes(2)));

	assert_equal(observer.get_execution_map().size(), worker_count);
}

void concurrencpp::tests::test_thread_pool_executor_bulk_submit() {
	object_observer observer;
	const size_t task_count = 40'000;
	const size_t worker_count = 6;
	auto executor = std::make_shared<thread_pool_executor>("threadpool", worker_count, std::chrono::seconds(10));
	executor_shutdowner shutdown(executor);

	std::vector<value_testing_stub> stubs;
	stubs.reserve(task_count);

	for (size_t i = 0; i < task_count; i++) {
		stubs.emplace_back(observer.get_testing_stub(i));
	}

	auto results = executor->template bulk_submit<value_testing_stub>(stubs);

	assert_true(observer.wait_execution_count(task_count, std::chrono::minutes(2)));
	assert_true(observer.wait_destruction_count(task_count, std::chrono::minutes(2)));

	assert_equal(observer.get_execution_map().size(), worker_count);

	for (size_t i = 0; i < task_count; i++) {
		assert_equal(results[i].get(), i);
	}
}

void concurrencpp::tests::test_thread_pool_executor_enqueue_algorithm() {
	//case 1 : if an idle thread exists, enqueue it to the idle thread
	{
		object_observer observer;
		const size_t worker_count = 6;
		auto executor = std::make_shared<thread_pool_executor>("threadpool", worker_count, std::chrono::seconds(10));
		executor_shutdowner shutdown(executor);

		for (size_t i = 0; i < worker_count; i++) {
			executor->post(observer.get_testing_stub(std::chrono::milliseconds(100)));
		}

		observer.wait_execution_count(worker_count, std::chrono::milliseconds(140));

		assert_equal(observer.get_execution_map().size(), worker_count);

		//make sure each task was executed on one task - a task wasn't posted to a working thread
		for (const auto& execution_thread : observer.get_execution_map()) {
			assert_equal(execution_thread.second, size_t(1));
		}
	}

	//case 2 : if (1) is false => if this is a thread-pool thread, enqueue to self
	{
		object_observer observer;
		auto wc = concurrencpp::details::wait_context::make();
		auto executor = std::make_shared<thread_pool_executor>("threadpool", 2, std::chrono::seconds(10));
		executor_shutdowner shutdown(executor);

		executor->post([wc] () {
			wc->wait();
		});

		const size_t task_count = 1'000;

		executor->post([&observer, executor, task_count] {
			for (size_t i = 0; i < task_count; i++) {
				executor->post(observer.get_testing_stub());
			}
		});

		observer.wait_execution_count(task_count, std::chrono::minutes(1));
		observer.wait_destruction_count(task_count, std::chrono::minutes(1));

		assert_equal(observer.get_execution_map().size(), size_t(1));

		wc->notify();
	}

	//case 3 : if (2) is false, choose a worker using round robin
	{
		const size_t task_count = 1'000;
		const size_t worker_count = 2;
		object_observer observer;
		auto wc = concurrencpp::details::wait_context::make();
		auto executor = std::make_shared<thread_pool_executor>("threadpool", worker_count, std::chrono::seconds(10));
		executor_shutdowner shutdown(executor);

		for (size_t i = 0; i < worker_count; i++) {
			executor->post([wc]() {
				wc->wait();
			});
		}

		for (size_t i = 0; i < task_count; i++) {
			executor->post(observer.get_testing_stub());
		}

		wc->notify();

		observer.wait_execution_count(task_count, std::chrono::minutes(1));
		observer.wait_destruction_count(task_count, std::chrono::minutes(1));

		const auto& execution_map = observer.get_execution_map();

		assert_equal(execution_map.size(), size_t(2));

		auto worker_it = execution_map.begin();
		assert_bigger(worker_it->second, task_count / 10);

		++worker_it;
		assert_bigger(worker_it->second, task_count / 10);
	}
}

void concurrencpp::tests::test_thread_pool_executor_dynamic_resizing() {
	//if the workers are only waiting - notify them
	{
		const size_t worker_count = 4;
		const size_t iterations = 4;
		const size_t task_count = 1'000;
		object_observer observer;
		auto executor = std::make_shared<thread_pool_executor>("threadpool", worker_count, std::chrono::seconds(5));
		executor_shutdowner shutdown(executor);

		for (size_t i = 0; i < iterations; i++) {
			for (size_t j = 0; j < task_count; j++) {
				executor->post(observer.get_testing_stub());
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(350));

			//in between, threads are waiting for an event (abort/task)
		}

		observer.wait_execution_count(task_count * iterations, std::chrono::minutes(1));
		observer.wait_destruction_count(task_count * iterations, std::chrono::minutes(1));

		//if all the tasks were launched by <<worker_count>> workers, then no new workers were injected
		assert_equal(observer.get_execution_map().size(), worker_count);
	}

	//case 2 : if max_idle_time reached, idle threads exit. new threads are injeced when new tasks arrive
	{
		const size_t iterations = 4;
		const size_t worker_count = 4;
		const size_t task_count = 4'000;
		object_observer observer;
		auto executor = std::make_shared<thread_pool_executor>("threadpool", worker_count, std::chrono::seconds(1));
		executor_shutdowner shutdown(executor);

		for (size_t i = 0; i < iterations; i++) {
			for (size_t j = 0; j < task_count; j++) {
				executor->post(observer.get_testing_stub());
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(2000));

			//in between, threads are idling
		}

		observer.wait_execution_count(task_count * iterations, std::chrono::minutes(1));
		observer.wait_execution_count(task_count * iterations, std::chrono::minutes(1));

		/*
			If all the tasks were executed by <<worker_count>> * iterations workers, then
			in every iteration a new set of threads was injected, meaning that the previous set of threads
			had exited.
		*/
		assert_equal(observer.get_execution_map().size(), worker_count * iterations);
	}
}

void concurrencpp::tests::test_thread_pool_executor() {
	tester tester("thread_pool_executor test");

	tester.add_step("name", test_thread_pool_executor_name);
	tester.add_step("shutdown", test_thread_pool_executor_shutdown);

	tester.add_step("post", test_thread_pool_executor_post);
	tester.add_step("submit", test_thread_pool_executor_submit);
	tester.add_step("bulk_post", test_thread_pool_executor_bulk_post);
	tester.add_step("bulk_submit", test_thread_pool_executor_bulk_submit);
	tester.add_step("enqueuing algorithm", test_thread_pool_executor_enqueue_algorithm);
	tester.add_step("dynamic resizing", test_thread_pool_executor_dynamic_resizing);

	tester.launch_test();
}