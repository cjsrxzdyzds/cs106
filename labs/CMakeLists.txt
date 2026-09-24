cmake_minimum_required(VERSION 3.16)
project(AP1400_2_Labs CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# -DLAB_SANITIZER=thread   ThreadSanitizer：检测数据竞争、锁顺序反转
# -DLAB_SANITIZER=address  AddressSanitizer + UBSan：检测越界、use-after-free、泄漏、UB
set(LAB_SANITIZER "" CACHE STRING "Sanitizer to enable: thread | address | (empty)")
if(LAB_SANITIZER STREQUAL "thread")
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
elseif(LAB_SANITIZER STREQUAL "address")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

find_package(Threads REQUIRED)
add_compile_options(-Wall -Wextra -Wpedantic)

enable_testing()

function(add_lab name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE Threads::Threads)
endfunction()

add_lab(lab0_race        lab0_race/race.cpp)
add_lab(lab1_matmul      lab1_matmul/matmul.cpp)
add_lab(lab2_mining      lab2_mining/mining.cpp)
add_lab(lab3_bst         lab3_bst/concurrent_bst.cpp)
add_lab(lab4_smart_ptr   lab4_smart_ptr/smart_ptr_test.cpp)
add_lab(lab5_thread_pool lab5_thread_pool/thread_pool_test.cpp)
add_lab(lab6_queue       lab6_queue/bounded_queue_test.cpp)

add_test(NAME lab0_race        COMMAND lab0_race)
add_test(NAME lab1_matmul      COMMAND lab1_matmul 192)
add_test(NAME lab2_mining      COMMAND lab2_mining)
add_test(NAME lab3_bst         COMMAND lab3_bst)
add_test(NAME lab4_smart_ptr   COMMAND lab4_smart_ptr)
add_test(NAME lab5_thread_pool COMMAND lab5_thread_pool)
add_test(NAME lab6_queue       COMMAND lab6_queue)
