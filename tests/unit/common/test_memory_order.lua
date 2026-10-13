-- The default workload moves four million items and exceeds the common 5 s limit under coverage.
neko_define_test(path.join(os.scriptdir(), "test_memory_order.cpp"), true, 60000)
