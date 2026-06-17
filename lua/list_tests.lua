import("core.base.task")
import("core.project.project")
import("actions.test.main", {rootdir = os.programdir(), alias = "test_action"})

task.run("config", {}, {disable_dump = true})

project.lock()
for name, info in table.orderpairs(test_action.get_tests()) do
    print(string.format("%-70s group=%s", name, info.group or ""))
end
project.unlock()