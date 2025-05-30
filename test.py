import subprocess
import sys

# --- 配置 ---
# 测试程序的可执行文件路径
TEST_EXECUTABLE = "./build/mylru_tests_mt"
# 您想要运行测试的次数
NUMBER_OF_RUNS = 100
# Google Test 输出中表示测试失败的关键词 (通常是这个)
GTEST_FAILURE_INDICATOR = "[  FAILED  ]"
# Google Test 输出中表示所有测试通过的关键词 (用于判断成功)
GTEST_PASSED_INDICATOR = "[  PASSED  ]"

def run_single_gtest():
    """
    运行一次 Google Test 可执行文件并返回其输出和返回码。
    返回: (return_code, stdout_output, stderr_output)
    """
    try:
        # 使用 subprocess.run 来执行命令
        # capture_output=True 会捕获标准输出和标准错误
        # text=True 会将输出解码为字符串
        # check=False 表示即使命令返回非零退出码，也不抛出 CalledProcessError
        process = subprocess.run(
            [TEST_EXECUTABLE],
            capture_output=True,
            text=True,
            check=False  # 重要：我们自己检查返回码
        )
        return process.returncode, process.stdout, process.stderr
    except FileNotFoundError:
        print(f"错误: 测试可执行文件未找到: {TEST_EXECUTABLE}")
        sys.exit(1)
    except Exception as e:
        print(f"运行测试时发生未知错误: {e}")
        return -1, "", str(e) # 返回一个非零错误码和错误信息

def main():
    any_failure_occurred = False
    failed_run_outputs = []

    print(f"将运行测试 '{TEST_EXECUTABLE}' {NUMBER_OF_RUNS} 次...\n")

    for i in range(NUMBER_OF_RUNS):
        print(f"--- 第 {i + 1}/{NUMBER_OF_RUNS} 次运行 ---")
        return_code, stdout_str, stderr_str = run_single_gtest()

        # Google Test 通常在所有测试通过时返回0，有失败时返回非0
        # 我们也检查输出中是否有 "[  FAILED  ]" 标志
        if return_code != 0 or GTEST_FAILURE_INDICATOR in stdout_str or GTEST_FAILURE_INDICATOR in stderr_str:
            any_failure_occurred = True
            failure_output = f"--- 运行 {i + 1} 失败 (返回码: {return_code}) ---\n"
            if stdout_str:
                failure_output += "标准输出:\n" + stdout_str + "\n"
            if stderr_str:
                failure_output += "标准错误:\n" + stderr_str + "\n"
            failure_output += "-------------------------------------------\n"
            failed_run_outputs.append(failure_output)
            print(f"测试失败 (返回码: {return_code}). 输出已记录。")
        else:
            print("测试通过。")
        print("-" * 30 + "\n")


    if any_failure_occurred:
        print("\n===== 检测到测试失败 =====")
        for output in failed_run_outputs:
            print(output)
    else:
        # 如果用户要求无失败时不输出，则这里可以留空或打印一个简单的成功消息
        # print("\n所有测试运行均成功。")
        pass # 根据要求，无失败时不输出

if __name__ == "__main__":
    main()
