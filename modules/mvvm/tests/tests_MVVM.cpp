/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mvvm/AsyncCommand.h>
#include <mvvm/Command.h>
#include <mvvm/Dispatcher.h>
#include <mvvm/Field.h>
#include <mvvm/ObservableObject.h>
#include <mvvm/ObservableList.h>
#include <mvvm/Validation.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace coral::mvvm;

/** A dispatcher that queues tasks and never claims the caller's thread. */
class RecordingDispatcher final : public Dispatcher {
 public:
  void post(std::function<void()> task) override {
    if (task) mTasks.push_back(std::move(task));
  }

  [[nodiscard]] bool isCurrentThread() const noexcept override {
    return false;
  }

  void drain() {
    std::vector<std::function<void()>> tasks;
    tasks.swap(mTasks);
    for (std::function<void()>& task: tasks) task();
  }

  [[nodiscard]] std::size_t pending() const noexcept {
    return mTasks.size();
  }

 private:
  std::vector<std::function<void()>> mTasks;
};

TEST(MvvmCommand, ExecutesAndRefreshesAvailability) {
  bool allowed = false;
  int runs = 0;
  int observed = 0;
  Command command([&] { ++runs; }, [&] { return allowed; });

  EXPECT_FALSE(command.canExecute());
  EXPECT_FALSE(command.execute());
  EXPECT_EQ(runs, 0);

  allowed = true;
  command.invalidateCanExecute();
  EXPECT_TRUE(command.canExecute());

  auto observation = command.observeExecuted([&] { ++observed; });
  EXPECT_TRUE(command.execute());
  EXPECT_EQ(runs, 1);
  EXPECT_EQ(observed, 1);
}

TEST(MvvmCommand, HandlerCanDisableItsOwnCommand) {
  bool allowed = true;
  Command command([&] { allowed = false; }, [&] { return allowed; });
  EXPECT_TRUE(command.canExecuteProperty().get());
  EXPECT_TRUE(command.execute());
  EXPECT_FALSE(command.canExecuteProperty().get()) << "execute refreshes canExecute";
}

TEST(MvvmAsyncCommand, CompletesOnlyOnce) {
  std::function<void()> done;
  AsyncCommand command([&](std::function<void()> finish) { done = std::move(finish); });
  int completed = 0;
  auto observation = command.completed().observe([&](const AsyncCommand::Completed&) { ++completed; });

  EXPECT_TRUE(command.execute());
  EXPECT_TRUE(command.running());
  EXPECT_FALSE(command.execute()) << "a running command refuses a second execution";

  ASSERT_TRUE(static_cast<bool>(done));
  done();
  EXPECT_FALSE(command.running());
  EXPECT_EQ(completed, 1);

  done();
  EXPECT_EQ(completed, 1) << "a duplicated completion is ignored";
}

TEST(MvvmAsyncCommand, ReportsAThrownFailure) {
  AsyncCommand command([](std::function<void()>) {
    throw std::runtime_error("disk full");
  });
  int failures = 0;
  auto observation = command.failed().observe([&](const AsyncCommand::Error& error) {
    ++failures;
    EXPECT_EQ(error.message, "disk full");
  });
  EXPECT_TRUE(command.execute());
  EXPECT_FALSE(command.running());
  EXPECT_EQ(command.errorProperty().get(), "disk full");
  EXPECT_EQ(failures, 1);
}

TEST(MvvmAsyncCommand, CancelIgnoresALateCompletion) {
  std::function<void()> done;
  AsyncCommand command([&](std::function<void()> finish) { done = std::move(finish); });
  int completed = 0;
  auto observation = command.completed().observe([&](const AsyncCommand::Completed&) { ++completed; });

  ASSERT_TRUE(command.execute());
  command.cancel();
  EXPECT_FALSE(command.running());
  ASSERT_TRUE(static_cast<bool>(done));
  done();
  EXPECT_EQ(completed, 0) << "a cancelled run must not report success";
  EXPECT_EQ(command.errorProperty().get(), "cancelled");
}

TEST(MvvmAsyncCommand, PostsCompletionThroughItsDispatcher) {
  auto dispatcher = std::make_shared<RecordingDispatcher>();
  std::function<void()> done;
  AsyncCommand command([&](std::function<void()> finish) { done = std::move(finish); }, {},
    dispatcher);
  EXPECT_TRUE(command.execute());
  done();
  EXPECT_TRUE(command.running()) << "the completion is queued, not applied inline";
  EXPECT_EQ(dispatcher->pending(), 1u);
  dispatcher->drain();
  EXPECT_FALSE(command.running());
}

TEST(MvvmList, ReportsStructuralChanges) {
  ObservableList<int> list;
  std::vector<std::string> events;
  auto inserted = list.observeInserted(
    [&](const ObservableList<int>::Inserted& event) {
      events.push_back("insert:" + std::to_string(event.index) + ":" + std::to_string(event.count));
    });
  auto removed = list.observeRemoved(
    [&](const ObservableList<int>::Removed& event) {
      events.push_back("remove:" + std::to_string(event.index) + ":" + std::to_string(event.count));
    });
  auto moved = list.observeMoved([&](const ObservableList<int>::Moved& event) {
    events.push_back("move:" + std::to_string(event.from) + ":" + std::to_string(event.to));
  });
  auto replaced = list.observeReplaced(
    [&](const ObservableList<int>::Replaced& event) {
      events.push_back("replace:" + std::to_string(event.index));
    });
  auto reset = list.observeReset([&](const ObservableList<int>::Reset&) {
    events.push_back("reset");
  });

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);
  list.replace(1, 20);
  list.move(0, 2);
  list.erase(1, 1);
  EXPECT_EQ(list.items(), (std::vector<int>{ 20, 1 }));
  EXPECT_EQ(events,
    (std::vector<std::string>{ "insert:0:1", "insert:1:1", "insert:2:1", "replace:1",
      "move:0:2", "remove:1:1" }));

  list.setAll({ 7, 8 });
  EXPECT_EQ(events.back(), "reset");
  EXPECT_EQ(list.size(), 2u);
  EXPECT_TRUE(list.at(0).has_value());
  EXPECT_EQ(*list.at(0), 7);
  EXPECT_FALSE(list.at(2).has_value());
}

TEST(MvvmValidation, FieldTracksValueErrorAndTouch) {
  Validation<std::string> validation;
  validation.add(required("required")).add(minLength(3, "too short"));
  Field<std::string> field("", validation);

  EXPECT_EQ(field.value().get(), "");
  EXPECT_FALSE(field.valid());
  EXPECT_EQ(field.error().get(), "required");
  EXPECT_FALSE(field.touched().get());

  field.setValue("ab");
  EXPECT_TRUE(field.touched().get());
  EXPECT_EQ(field.error().get(), "too short");
  EXPECT_FALSE(field.validate());

  field.setValue("abc");
  EXPECT_TRUE(field.valid());
  EXPECT_EQ(field.error().get(), "");

  field.reset("");
  EXPECT_FALSE(field.touched().get());
  EXPECT_TRUE(field.valid()) << "reset clears the message until the field is edited again";
}

TEST(MvvmObservableObject, PostUsesTheDispatcherWhenInstalled) {
  auto dispatcher = std::make_shared<RecordingDispatcher>();
  ObservableObject object;
  object.setDispatcher(dispatcher);
  int calls = 0;
  object.post([&] { ++calls; });
  EXPECT_EQ(calls, 0);
  dispatcher->drain();
  EXPECT_EQ(calls, 1);
}

} // namespace
