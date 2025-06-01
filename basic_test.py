import subprocess
import os
import shutil
import csv
import re # 用于解析输出
import pandas as pd
from tabulate import tabulate
import logging
from datetime import datetime

# --- 用户配置 ---
PROJECT_ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".")) # 假设脚本在项目根目录
BUILD_DIR_BASE_NAME = "build_scenario"
NUM_RUNS = 10  # 每个配置运行的次数

# 设置日志记录
LOG_DIR = "logs"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)
LOG_FILENAME = os.path.join(LOG_DIR, f"cpp_output_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log")

# 配置日志记录器
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILENAME, encoding='utf-8'),
        logging.StreamHandler()
    ]
)

CMAKE_BUILD_TYPE = "Release"  
# CMAKE_BUILD_TYPE = "Debug"  

FIXED_K_NUM_SEG_BITS = 4 

TEST_CONFIGURATIONS = [
    {
        "name": "NoResizer_Libcuckoo",
        "mt_features": "PRE_ALLOCATE;USE_LIBCUCKOO",
        "mt_ht_features": "PRE_ALLOCATE;USE_LIBCUCKOO;USE_HHVM"
    },
    {
        "name": "NoResizer_MyHashTable",
        "mt_features": "PRE_ALLOCATE;USE_MY_HASH_TABLE",
        "mt_ht_features": "PRE_ALLOCATE;USE_MY_HASH_TABLE;USE_HHVM"
    },
    {
        "name": "WithResizer_MyHashTable",
        "mt_features": "PRE_ALLOCATE;USE_MY_HASH_TABLE;USE_HASH_RESIZER",
        "mt_ht_features": "PRE_ALLOCATE;USE_MY_HASH_TABLE;USE_HHVM;USE_HASH_RESIZER"
    },
    {
        "name": "WithResizer_SharedMutex_MyHashTable",
        "mt_features": "PRE_ALLOCATE;USE_MY_HASH_TABLE;USE_HASH_RESIZER;USE_SHARED_LATCH",
        "mt_ht_features": "PRE_ALLOCATE;USE_MY_HASH_TABLE;USE_HHVM;USE_HASH_RESIZER;USE_SHARED_LATCH"
    },
    {
        "name": "NoResizer_SegHashTable", 
        "mt_features": "PRE_ALLOCATE;USE_SEG_HASH_TABLE",
        "mt_ht_features": "PRE_ALLOCATE;USE_SEG_HASH_TABLE;USE_HHVM"
    }
]

# 要为每个配置运行的测试可执行文件
EXECUTABLES_TO_RUN = ["mylru_tests_mt", "mylru_tests_mt_ht"]

# Google Test 输出指示符
GTEST_FAILURE_INDICATOR = "[  FAILED  ]"
GTEST_PASSED_INDICATOR = "[  PASSED  ]"

# --- 辅助函数 ---
def parse_gtest_output(output_str):
    """
    解析 Google Test 输出，提取每个测试用例的名称和指标。
    返回一个字典列表，每个字典包含:
    { "GTest_Case_Name": "...", "Throughput (ops/sec)": ..., "Hit Ratio (%)": ..., "Actual Run Time (s)": ... }
    """
    parsed_test_cases = []
    
    # 首先按分隔符 "----------------------------------------" 分割，尝试找到每个测试的输出块
    output_blocks = output_str.split("----------------------------------------")
    
    current_test_name_from_gtest_run_line = None

    for i, block in enumerate(output_blocks):
        block = block.strip()
        if not block:
            continue

        # 尝试从 GTest 的 [ RUN      ] 行提取测试名
        run_line_match = re.search(r"\[ RUN      \] ([\w\.]+)", block)
        if run_line_match:
            current_test_name_from_gtest_run_line = run_line_match.group(1)

        # 查找您自定义的 "Test: " 标记
        test_name_match = re.search(r"Test: (.+)", block)
        if test_name_match:
            gtest_case_name = test_name_match.group(1).strip()
            metrics = {}
            try:
                # 修改正则表达式以正确处理科学计数法
                throughput_match = re.search(r"Throughput(?: \(Planned Ops\))?: ([\d\.]+(?:[eE][+-]?\d+)?)\s*ops/sec", block)
                if throughput_match: 
                    metrics["Throughput (ops/sec)"] = float(throughput_match.group(1))
                
                hit_ratio_match = re.search(r"Hit Ratio(?: \(Finds\))?: ([\d\.]+)\s*%", block)
                if hit_ratio_match: 
                    metrics["Hit Ratio (%)"] = float(hit_ratio_match.group(1))
                
                run_time_match = re.search(r"Actual Run Time: ([\d\.]+)\s*seconds", block)
                if run_time_match: 
                    metrics["Actual Run Time (s)"] = float(run_time_match.group(1))
                
                parsed_test_cases.append({
                    "GTest_Case_Name": gtest_case_name, 
                    **metrics
                })
            except Exception as e:
                print(f"    解析指标时出错 (Test: {gtest_case_name}): {e}")
                parsed_test_cases.append({ "GTest_Case_Name": gtest_case_name, "Error": str(e)})
    
    # 如果没有找到 "Test: " 标记，但 GTest 运行了，至少记录 GTest 的测试名
    if not parsed_test_cases and current_test_name_from_gtest_run_line:
        overall_metrics = {}
        try:
            # 修改正则表达式以正确处理科学计数法
            throughput_match = re.search(r"Throughput(?: \(Planned Ops\))?: ([\d\.]+(?:[eE][+-]?\d+)?)\s*ops/sec", output_str)
            if throughput_match: 
                overall_metrics["Throughput (ops/sec)"] = float(throughput_match.group(1))
            
            hit_ratio_match = re.search(r"Hit Ratio(?: \(Finds\))?: ([\d\.]+)\s*%", output_str)
            if hit_ratio_match: 
                overall_metrics["Hit Ratio (%)"] = float(hit_ratio_match.group(1))
            
            run_time_match = re.search(r"Actual Run Time: ([\d\.]+)\s*seconds", output_str)
            if run_time_match: 
                overall_metrics["Actual Run Time (s)"] = float(run_time_match.group(1))
        except Exception:
            pass
        
        gtest_end_match = re.search(r"\[\s*(?:OK|FAILED)\s*\] ([\w\.]+)", output_str)
        if gtest_end_match:
             parsed_test_cases.append({"GTest_Case_Name": gtest_end_match.group(1), **overall_metrics})
        elif current_test_name_from_gtest_run_line:
             parsed_test_cases.append({"GTest_Case_Name": current_test_name_from_gtest_run_line, **overall_metrics})

    if not parsed_test_cases:
        print(f"    警告: 未能从以下输出中解析出任何测试用例结果块:\n{output_str[:500]}...")

    return parsed_test_cases


def run_command(cmd_list, working_dir=None, step_name="Command", timeout_seconds=600): # 增加超时
    logging.info(f"执行 {step_name}: {' '.join(cmd_list)}")
    try:
        process = subprocess.run(
            cmd_list, cwd=working_dir, capture_output=True, text=True, check=False, timeout=timeout_seconds
        )
        if process.returncode != 0 and step_name != "运行测试": # 对于测试运行，我们单独判断成功失败
            logging.error(f"{step_name} 失败 (返回码: {process.returncode}).")
            logging.info("标准输出:\n%s", process.stdout)
            if process.stderr: logging.info("标准错误:\n%s", process.stderr)
            return False, process.stdout, process.stderr
        
        # 如果是测试运行，检查并显示哈希表类型输出
        if step_name.startswith("运行"):
            # 查找哈希表类型输出
            hash_table_output = re.search(r"Using (.*?) hash table", process.stdout)
            if hash_table_output:
                logging.info("\n哈希表类型: %s", hash_table_output.group(1))
        
        # 记录所有输出到日志文件
        logging.info("命令输出:\n%s", process.stdout)
        if process.stderr:
            logging.info("命令错误输出:\n%s", process.stderr)
        
        return True, process.stdout, process.stderr
    except FileNotFoundError:
        logging.error("错误: 命令 '%s' 未找到.", cmd_list[0])
        return False, "", f"Command not found: {cmd_list[0]}"
    except subprocess.TimeoutExpired:
        logging.error("错误: %s 超时 (%d 秒).", step_name, timeout_seconds)
        return False, "", f"{step_name} timed out."
    except Exception as e:
        logging.error("运行命令时发生未知错误: %s", e)
        return False, "", str(e)

def create_performance_table(all_run_data):
    """
    创建性能测试结果的可视化表格，显示多次运行的平均值
    """
    # 创建结果字典
    results = {
        "Single LRU": {},
        "SegLRU": {},
        "SegLRU HT": {}
    }
    
    # 用于存储多次运行的数据
    run_data = {
        "Single LRU": {},
        "SegLRU": {},
        "SegLRU HT": {}
    }
    
    # 遍历所有运行数据
    for data in all_run_data:
        if data["Status"] != "PASSED":
            continue
            
        config_name = data["Config_Name"]
        executable = data["Executable"]
        test_case = data["GTest_Case_Name"]
        throughput = data.get("Throughput (ops/sec)")
        hit_ratio = data.get("Hit Ratio (%)")
        actual_run_time = data.get("Actual Run Time (s)")
        
        # 根据可执行文件和测试用例名称分类
        if executable == "mylru_tests_mt" and "BenchMark Test (Single LRU)" in test_case:
            category = "Single LRU"
        elif executable == "mylru_tests_mt" and "Randomized Mixed Operations Test (SegLRUCache)" in test_case:
            category = "SegLRU"
        elif executable == "mylru_tests_mt_ht" and "Randomized Mixed Operations Test (SegLRUCache)" in test_case:
            category = "SegLRU HT"
        else:
            continue

        # 初始化配置的数据结构
        if config_name not in run_data[category]:
            run_data[category][config_name] = {
                "Throughput": [],
                "Hit Ratio": [],
                "Actual Run Time": []
            }
        
        # 添加数据
        if throughput is not None:
            run_data[category][config_name]["Throughput"].append(throughput)
        if hit_ratio is not None:
            run_data[category][config_name]["Hit Ratio"].append(hit_ratio)
        if actual_run_time is not None:
            run_data[category][config_name]["Actual Run Time"].append(actual_run_time)
    
    # 计算平均值
    for category in run_data:
        for config_name, metrics in run_data[category].items():
            avg_throughput = sum(metrics["Throughput"]) / len(metrics["Throughput"]) if metrics["Throughput"] else None
            avg_hit_ratio = sum(metrics["Hit Ratio"]) / len(metrics["Hit Ratio"]) if metrics["Hit Ratio"] else None
            avg_run_time = sum(metrics["Actual Run Time"]) / len(metrics["Actual Run Time"]) if metrics["Actual Run Time"] else None
            
            results[category][config_name] = {
                "Throughput": avg_throughput,
                "Hit Ratio": avg_hit_ratio,
                "Actual Run Time": avg_run_time
            }
    
    # 创建表格数据
    table_data = []
    headers = ["Implementation", "Configuration", "Avg Throughput (ops/sec)", "Avg Hit Ratio (%)", "Avg Run Time (s)"]
    
    for impl, configs in results.items():
        for config, metrics in configs.items():
            # 将科学计数法转换为整数
            throughput_str = "N/A"
            if metrics['Throughput']:
                throughput_int = int(metrics['Throughput'])
                throughput_str = f"{throughput_int:,}"  # 添加千位分隔符
            
            table_data.append([
                impl,
                config,
                throughput_str,
                f"{metrics['Hit Ratio']:.2f}" if metrics['Hit Ratio'] else "N/A",
                f"{metrics['Actual Run Time']:.2f}" if metrics['Actual Run Time'] else "N/A"
            ])
    
    # 使用 tabulate 生成表格
    table = tabulate(table_data, headers=headers, tablefmt="grid")
    
    # 打印表格
    print("\n性能测试结果表格 (平均值):")
    print(table)
    
    # 保存到文件
    with open("performance_results.txt", "w") as f:
        f.write("性能测试结果表格 (平均值):\n")
        f.write(table)
    
    print("\n结果已保存到 performance_results.txt")

# --- 主测试逻辑 ---
def main():
    logging.info("开始测试运行")
    all_run_data = []
    csv_filename = "lru_benchmark_final_results.csv"
    csv_fieldnames = [
        "Config_Name", "kNumSegBits", "SegNum", "Executable", "GTest_Case_Name", "Run_Number", "Status",
        "Throughput (ops/sec)", "Hit Ratio (%)", "Actual Run Time (s)", "FailedOutput"
    ]

    if not os.path.isdir(os.path.join(PROJECT_ROOT_DIR, "src")) or \
       not os.path.isdir(os.path.join(PROJECT_ROOT_DIR, "test")) or \
       not os.path.isfile(os.path.join(PROJECT_ROOT_DIR, "CMakeLists.txt")):
        logging.error("错误：PROJECT_ROOT_DIR ('%s') 似乎不是有效的项目根目录。请检查路径。", PROJECT_ROOT_DIR)
        return

    k_bits = FIXED_K_NUM_SEG_BITS # 使用固定的 k_bits
    seg_num = 1 << k_bits
    
    for config_info in TEST_CONFIGURATIONS:
        config_name = config_info["name"]
        mt_features = config_info["mt_features"]
        mt_ht_features = config_info["mt_ht_features"]
        
        current_run_id_prefix = f"{config_name}" # 构建目录不再包含 k_bits
        print(f"\n===== 开始测试配置: {config_name} (固定 kNumSegBits={k_bits}, segNum={seg_num}) =====")

        build_path = os.path.join(PROJECT_ROOT_DIR, f"{BUILD_DIR_BASE_NAME}_{current_run_id_prefix}")

        if os.path.exists(build_path):
            print(f"  清理旧的构建目录: {build_path}")
            shutil.rmtree(build_path)
        os.makedirs(build_path)
        print(f"  已创建构建目录: {build_path}")

        cmake_cmd = [
            "cmake",
            f"-DCMAKE_BUILD_TYPE={CMAKE_BUILD_TYPE}",
            f"-DK_NUM_SEG_BITS_FROM_CMAKE={k_bits}", 
            f"-DMYLRU_TESTS_MT_FEATURES={mt_features}",
            f"-DMYLRU_TESTS_MT_HT_FEATURES={mt_ht_features}",
            "-S", PROJECT_ROOT_DIR,
            "-B", build_path
        ]
        success, cmake_stdout, cmake_stderr = run_command(cmake_cmd, step_name="CMake 配置")
        if not success:
            all_run_data.append({
                "Config_Name": config_name, "kNumSegBits": k_bits, "SegNum": seg_num,
                "Executable": "N/A (CMake Failed)", "GTest_Case_Name": "N/A", "Run_Number": 0,
                "Status": "CMAKE_ERROR", "FailedOutput": cmake_stdout + cmake_stderr
            })
            continue

        build_cmd = ["cmake", "--build", build_path, "--parallel"]
        success, build_stdout, build_stderr = run_command(build_cmd, step_name="构建")
        if not success:
            all_run_data.append({
                "Config_Name": config_name, "kNumSegBits": k_bits, "SegNum": seg_num,
                "Executable": "N/A (Build Failed)", "GTest_Case_Name": "N/A", "Run_Number": 0,
                "Status": "BUILD_ERROR", "FailedOutput": build_stdout + build_stderr
            })
            continue

        for exe_name in EXECUTABLES_TO_RUN:
            test_exe_path = os.path.join(build_path, exe_name)
            if not os.path.isfile(test_exe_path):
                print(f"  警告: 测试可执行文件未找到: {test_exe_path}")
                all_run_data.append({
                    "Config_Name": config_name, "kNumSegBits": k_bits, "SegNum": seg_num,
                    "Executable": exe_name, "GTest_Case_Name": "N/A", "Run_Number": 0,
                    "Status": "EXE_NOT_FOUND", "FailedOutput": f"{exe_name} not found"
                })
                continue

            print(f"  运行测试: {exe_name}")
            
            # 运行多次测试
            for run_number in range(1, NUM_RUNS + 1):
                print(f"    运行 #{run_number}/{NUM_RUNS}")
                # 对于测试运行，即使返回码非0，我们也继续解析输出，因为GTest失败会返回非0
                run_success, stdout_str, stderr_str = run_command([test_exe_path], step_name=f"运行 {exe_name} (运行 #{run_number})")
                
                full_output = stdout_str + stderr_str
                parsed_metrics_list = parse_gtest_output(full_output)

                if not parsed_metrics_list: # 如果解析器未能提取任何测试用例块
                     all_run_data.append({
                        "Config_Name": config_name, "kNumSegBits": k_bits, "SegNum": seg_num,
                        "Executable": exe_name, "GTest_Case_Name": "Unknown (Parse Failed)", "Run_Number": run_number,
                        "Status": "PARSE_ERROR", "FailedOutput": full_output[:1000] + "..." if len(full_output) > 1000 else full_output
                    })
                     print(f"      测试 {exe_name} 的输出解析失败。")
                     continue

                for metrics_data in parsed_metrics_list:
                    gtest_case_name = metrics_data.get("GTest_Case_Name", "Unknown Test Case")
                    test_status = "PASSED" # 先假设通过

                    # GTest 失败判断：可执行文件返回码非0，或者输出中包含失败标志
                    if not run_success or GTEST_FAILURE_INDICATOR in full_output:
                        test_status = "FAILED"
                    
                    # 如果解析时遇到错误，也标记为失败
                    if "Error" in metrics_data:
                        test_status = "PARSE_ERROR"

                    all_run_data.append({
                        "Config_Name": config_name, "kNumSegBits": k_bits, "SegNum": seg_num,
                        "Executable": exe_name, "GTest_Case_Name": gtest_case_name, "Run_Number": run_number,
                        "Status": test_status,
                        "Throughput (ops/sec)": metrics_data.get("Throughput (ops/sec)"),
                        "Hit Ratio (%)": metrics_data.get("Hit Ratio (%)"),
                        "Actual Run Time (s)": metrics_data.get("Actual Run Time (s)"),
                        "FailedOutput": full_output if test_status == "FAILED" else ""
                    })
                    print(f"      可执行文件: {exe_name}, 测试用例: {gtest_case_name}, 状态: {test_status}, 指标: { {k:v for k,v in metrics_data.items() if k != 'GTest_Case_Name'} }")

    print("\n\n===== 所有测试运行总结写入CSV =====")
    with open(csv_filename, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=csv_fieldnames)
        writer.writeheader()
        for row_data in all_run_data:
            row_to_write = {field: row_data.get(field, "") for field in csv_fieldnames}
            if row_to_write["FailedOutput"]:
                 row_to_write["FailedOutput"] = row_to_write["FailedOutput"][:1000] + "..." if len(row_to_write["FailedOutput"]) > 1000 else row_to_write["FailedOutput"]
            writer.writerow(row_to_write)
            
    print(f"\n所有结果已保存到 {csv_filename}")
    
    # 创建性能表格
    create_performance_table(all_run_data)

if __name__ == "__main__":
    main()
