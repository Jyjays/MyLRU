import subprocess
import os
import shutil
import re 
import csv 
import matplotlib.pyplot as plt
import matplotlib as mpl
import logging
from datetime import datetime

PROJECT_ROOT_DIR = "/home/jyjays/LAB/MyLRU"  
BUILD_DIR_BASE_NAME = "build_kbits"       
CMAKE_BUILD_TYPE = "Release"             
K_NUM_SEG_BITS_TO_TEST = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10] 

# 设置日志记录
LOG_DIR = "logs"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)
LOG_FILENAME = os.path.join(LOG_DIR, f"seg_test_output_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log")

# 配置日志记录器
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILENAME, encoding='utf-8'),
        logging.StreamHandler()
    ]
)

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
TEST_EXECUTABLES = [
    "mylru_tests_mt",
    "mylru_tests_mt_ht"
]

GTEST_FAILURE_INDICATOR = "[  FAILED  ]"
GTEST_PASSED_INDICATOR = "[  PASSED  ]"

# --- 辅助函数 ---

def parse_gtest_output(output_str, executable_name):
    """
    解析 Google Test 输出，提取 SegLRUCache 测试的指标。
    返回一个包含指标的字典。
    """
    results = {}
    try:
        # 查找 SegLRUCache 测试的输出块
        seglru_match = re.search(r"Test: Randomized Mixed Operations Test \(SegLRUCache\)(.*?)(?=Test:|$)", 
                                output_str, re.DOTALL)
        
        if seglru_match:
            test_output = seglru_match.group(1)
            
            # 尝试提取吞吐量 (Ops/sec)
            throughput_match = re.search(r"Throughput(?: \(Planned Ops\))?: ([\d\.]+)\s*ops/sec", test_output)
            if throughput_match:
                results["Throughput (ops/sec)"] = float(throughput_match.group(1))

            # 尝试提取命中率 (%)
            hit_ratio_match = re.search(r"Hit Ratio(?: \(Finds\))?: ([\d\.]+)\s*%", test_output)
            if hit_ratio_match:
                results["Hit Ratio (%)"] = float(hit_ratio_match.group(1))
            
            # 尝试提取实际运行时间 (seconds)
            run_time_match = re.search(r"Actual Run Time: ([\d\.]+)\s*seconds", test_output)
            if run_time_match:
                results["Actual Run Time (s)"] = float(run_time_match.group(1))
        else:
            logging.warning(f"未找到 {executable_name} 的 SegLRUCache 测试输出")

    except Exception as e:
        logging.error(f"解析 {executable_name} 输出时出错: {e}")
    return results

def run_command(cmd_list, working_dir=None, step_name="Command"):
    """
    运行一个命令并返回其成功状态和输出。
    """
    logging.info(f"执行 {step_name}: {' '.join(cmd_list)}")
    try:
        process = subprocess.run(
            cmd_list,
            cwd=working_dir,
            capture_output=True,
            text=True,
            check=False
        )
        if process.returncode != 0:
            logging.error(f"{step_name} 失败 (返回码: {process.returncode}).")
            logging.info("标准输出:\n%s", process.stdout)
            logging.info("标准错误:\n%s", process.stderr)
            return False, process.stdout, process.stderr
        
        # 记录所有输出到日志文件
        logging.info("命令输出:\n%s", process.stdout)
        if process.stderr:
            logging.info("命令错误输出:\n%s", process.stderr)
            
        return True, process.stdout, process.stderr
    except FileNotFoundError:
        logging.error("错误: 命令 '%s' 未找到.", cmd_list[0])
        return False, "", f"Command not found: {cmd_list[0]}"
    except Exception as e:
        logging.error("运行命令时发生未知错误: %s", e)
        return False, "", str(e)


def plot_throughput_comparison(csv_filename):
    data = []
    with open(csv_filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)
    
    throughput_data = {}
    for row in data:
        exe = row['Executable']
        seg_num = int(row['Config_SegNum'])
        throughput = float(row['Throughput (ops/sec)']) if row['Throughput (ops/sec)'] else 0
        
        if exe not in throughput_data:
            throughput_data[exe] = {'seg_nums': [], 'throughputs': []}
        
        throughput_data[exe]['seg_nums'].append(seg_num)
        throughput_data[exe]['throughputs'].append(throughput)
    
    plt.figure(figsize=(8, 6), dpi=300)
    
    for exe, data in throughput_data.items():
        sorted_indices = sorted(range(len(data['seg_nums'])), key=lambda i: data['seg_nums'][i])
        seg_nums = [data['seg_nums'][i] for i in sorted_indices]
        throughputs = [data['throughputs'][i] for i in sorted_indices]
        
        x_positions = list(range(len(seg_nums)))
        
        label = "Seg LRU" if exe == "mylru_tests_mt" else "Seg LRU HT"
        plt.plot(x_positions, throughputs, marker='o', label=label, linewidth=2, markersize=8)
    
    plt.xticks(range(len(K_NUM_SEG_BITS_TO_TEST)), [1 << i for i in K_NUM_SEG_BITS_TO_TEST])
    
    plt.xlabel('Segment Numbers', fontsize=12)
    plt.ylabel('Throughput (ops/sec)', fontsize=12)
    plt.title('Throughput Comparison with Different Segment Numbers', fontsize=14)
    
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend(fontsize=10)
    plt.tight_layout()
    
    plt.savefig('throughput_comparison.pdf', format='pdf', bbox_inches='tight')
    plt.savefig('throughput_comparison.png', format='png', bbox_inches='tight', dpi=300)
    plt.close()

# --- 主测试逻辑 ---

def main():
    logging.info("开始分段测试运行")
    all_results_summary = []
    config = TEST_CONFIGURATIONS[1]  
    mt_features = config["mt_features"]
    mt_ht_features = config["mt_ht_features"]
        
    for k_bits in K_NUM_SEG_BITS_TO_TEST:
        seg_num = 1 << k_bits
        current_config_name = f"kNumSegBits={k_bits} (segNum={seg_num})"
        logging.info(f"\n===== 开始测试配置: {current_config_name} =====")

        build_path = os.path.join(PROJECT_ROOT_DIR, f"{BUILD_DIR_BASE_NAME}_{k_bits}")

        if os.path.exists(build_path):
            logging.info(f"清理旧的构建目录: {build_path}")
            shutil.rmtree(build_path)
        os.makedirs(build_path)
        logging.info(f"已创建构建目录: {build_path}")

        cmake_cmd = [
            "cmake",
            f"-DCMAKE_BUILD_TYPE={CMAKE_BUILD_TYPE}",
            f"-DK_NUM_SEG_BITS_FROM_CMAKE={k_bits}", 
            f"-DMYLRU_TESTS_MT_FEATURES={mt_features}",
            f"-DMYLRU_TESTS_MT_HT_FEATURES={mt_ht_features}",
            "-S", PROJECT_ROOT_DIR,
            "-B", build_path
        ]
        success, _, _ = run_command(cmake_cmd, step_name="CMake 配置")
        if not success:
            all_results_summary.append({
                "Config": current_config_name, "Executable": "N/A (CMake Failed)", 
                "Status": "CMAKE_ERROR", "Metrics": {}
            })
            continue 

        build_cmd = ["cmake", "--build", build_path, "--parallel"] 
        success, _, _ = run_command(build_cmd, step_name="构建")
        if not success:
            all_results_summary.append({
                "Config": current_config_name, "Executable": "N/A (Build Failed)", 
                "Status": "BUILD_ERROR", "Metrics": {}
            })
            continue

        for exe_name in TEST_EXECUTABLES:
            test_exe_path = os.path.join(build_path, exe_name)
            
            if not os.path.isfile(test_exe_path):
                logging.warning(f"测试可执行文件未找到: {test_exe_path}")
                all_results_summary.append({
                    "Config": current_config_name, "Executable": exe_name, 
                    "Status": "EXE_NOT_FOUND", "Metrics": {}
                })
                continue

            logging.info(f"运行测试: {exe_name}")
            success, stdout_str, stderr_str = run_command([test_exe_path], step_name=f"运行 {exe_name}")
            
            metrics = parse_gtest_output(stdout_str + stderr_str, exe_name)
            test_status = "PASSED"
          
            if not success or GTEST_FAILURE_INDICATOR in stdout_str or GTEST_FAILURE_INDICATOR in stderr_str:
                test_status = "FAILED"
                logging.error(f"测试 {exe_name} 失败。输出已记录。")
                
                if success and (GTEST_FAILURE_INDICATOR in stdout_str or GTEST_FAILURE_INDICATOR in stderr_str):
                    success = False

            all_results_summary.append({
                "Config": current_config_name,
                "Executable": exe_name,
                "Status": test_status,
                "Metrics": metrics,
                "RawOutput": stdout_str + stderr_str if not success else ""
            })
            logging.info(f"{exe_name} 完成，状态: {test_status}, 指标: {metrics}")

    logging.info("\n\n===== 所有测试运行总结 =====")
    csv_filename = "lru_segnum_benchmark_results.csv"
    csv_fieldnames = ["Config_kNumSegBits", "Config_SegNum", "Executable", "Status", 
                      "Throughput (ops/sec)", "Hit Ratio (%)", "Actual Run Time (s)", "FailedOutput"]
    
    with open(csv_filename, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=csv_fieldnames)
        writer.writeheader()

        for result in all_results_summary:
            k_bits_str = result["Config"].split('=')[1].split(' ')[0]
            seg_num_val = 1 << int(k_bits_str)
            
            row_to_write = {
                "Config_kNumSegBits": k_bits_str,
                "Config_SegNum": seg_num_val,
                "Executable": result["Executable"],
                "Status": result["Status"],
                "Throughput (ops/sec)": result["Metrics"].get("Throughput (ops/sec)", ""),
                "Hit Ratio (%)": result["Metrics"].get("Hit Ratio (%)", ""),
                "Actual Run Time (s)": result["Metrics"].get("Actual Run Time (s)", ""),
                "FailedOutput": result.get("RawOutput", "") if result["Status"] == "FAILED" else ""
            }
            writer.writerow(row_to_write)
            
            logging.info(f"配置: {result['Config']}, 可执行文件: {result['Executable']}, 状态: {result['Status']}")
            if result["Status"] == "FAILED":
                logging.error(f"失败输出:\n{result.get('RawOutput', 'N/A')[:500]}...")
            else:
                logging.info(f"指标: {result['Metrics']}")
            logging.info("-" * 30)
            
    logging.info(f"\n所有结果已保存到 {csv_filename}")
    plot_throughput_comparison(csv_filename)

if __name__ == "__main__":
    main()
