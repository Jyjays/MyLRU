import subprocess
import os
import shutil
import re 
import csv 
import matplotlib.pyplot as plt
import matplotlib as mpl

PROJECT_ROOT_DIR = "/home/jyjays/LAB/MyLRU"  
BUILD_DIR_BASE_NAME = "build_kbits"       
CMAKE_BUILD_TYPE = "Release"             
K_NUM_SEG_BITS_TO_TEST = [0, 1, 2, 3, 4, 5, 6, 7] 

TEST_EXECUTABLES = [
    "mylru_tests_mt",
    "mylru_tests_mt_ht"
]

GTEST_FAILURE_INDICATOR = "[  FAILED  ]"
GTEST_PASSED_INDICATOR = "[  PASSED  ]"

# --- 辅助函数 ---

def parse_gtest_output(output_str):
    """
    一个简单的解析器，尝试从 Google Test 输出中提取关键指标。
    您需要根据您的 printEvaluationResult 函数的实际输出格式来调整它。
    返回一个包含指标的字典。
    """
    results = {}
    try:
        # 尝试提取吞吐量 (Ops/sec)
        throughput_match = re.search(r"Throughput(?: \(Planned Ops\))?: ([\d\.]+)\s*ops/sec", output_str)
        if throughput_match:
            results["Throughput (ops/sec)"] = float(throughput_match.group(1))

        # 尝试提取命中率 (%)
        hit_ratio_match = re.search(r"Hit Ratio(?: \(Finds\))?: ([\d\.]+)\s*%", output_str)
        if hit_ratio_match:
            results["Hit Ratio (%)"] = float(hit_ratio_match.group(1))
        
        # 尝试提取实际运行时间 (seconds)
        run_time_match = re.search(r"Actual Run Time: ([\d\.]+)\s*seconds", output_str)
        if run_time_match:
            results["Actual Run Time (s)"] = float(run_time_match.group(1))

    except Exception as e:
        print(f"    解析输出时出错: {e}")
    return results

def run_command(cmd_list, working_dir=None, step_name="Command"):
    """
    运行一个命令并返回其成功状态和输出。
    """
    print(f"  执行 {step_name}: {' '.join(cmd_list)}")
    try:
        process = subprocess.run(
            cmd_list,
            cwd=working_dir,
            capture_output=True,
            text=True,
            check=False
        )
        if process.returncode != 0:
            print(f"  {step_name} 失败 (返回码: {process.returncode}).")
            print("  标准输出:\n", process.stdout)
            print("  标准错误:\n", process.stderr)
            return False, process.stdout, process.stderr
        return True, process.stdout, process.stderr
    except FileNotFoundError:
        print(f"  错误: 命令 '{cmd_list[0]}' 未找到.")
        return False, "", f"Command not found: {cmd_list[0]}"
    except Exception as e:
        print(f"  运行命令时发生未知错误: {e}")
        return False, "", str(e)


def plot_throughput_comparison(csv_filename):
    # 读取CSV文件
    data = []
    with open(csv_filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)
    
    # 按可执行文件和分段数分组数据
    throughput_data = {}
    for row in data:
        exe = row['Executable']
        seg_num = int(row['Config_SegNum'])
        throughput = float(row['Throughput (ops/sec)']) if row['Throughput (ops/sec)'] else 0
        
        if exe not in throughput_data:
            throughput_data[exe] = {'seg_nums': [], 'throughputs': []}
        
        throughput_data[exe]['seg_nums'].append(seg_num)
        throughput_data[exe]['throughputs'].append(throughput)
    
    # 创建图形，设置更高的DPI以获得更好的质量
    plt.figure(figsize=(8, 6), dpi=300)
    
    # 为每个可执行文件绘制一条线
    for exe, data in throughput_data.items():
        # 对数据按分段数排序
        sorted_indices = sorted(range(len(data['seg_nums'])), key=lambda i: data['seg_nums'][i])
        seg_nums = [data['seg_nums'][i] for i in sorted_indices]
        throughputs = [data['throughputs'][i] for i in sorted_indices]
        
        # 创建均匀分布的x轴位置
        x_positions = list(range(len(seg_nums)))
        
        # 绘制折线
        label = "Seg LRU" if exe == "mylru_tests_mt" else "Seg LRU TL"
        plt.plot(x_positions, throughputs, marker='o', label=label, linewidth=2, markersize=8)
    
    # 设置x轴刻度和标签
    plt.xticks(range(len(K_NUM_SEG_BITS_TO_TEST)), [1 << i for i in K_NUM_SEG_BITS_TO_TEST])
    
    # 设置标签和标题
    plt.xlabel('Segment Numbers', fontsize=12)
    plt.ylabel('Throughput (ops/sec)', fontsize=12)
    plt.title('Throughput Comparison with Different Segment Numbers', fontsize=14)
    
    # 添加网格
    plt.grid(True, linestyle='--', alpha=0.7)
    
    # 添加图例
    plt.legend(fontsize=10)
    
    # 调整布局
    plt.tight_layout()
    
    # 保存为矢量图格式（PDF）
    plt.savefig('throughput_comparison.pdf', format='pdf', bbox_inches='tight')
    # 同时保存一个PNG版本用于预览
    plt.savefig('throughput_comparison.png', format='png', bbox_inches='tight', dpi=300)
    plt.close()

# --- 主测试逻辑 ---

def main():
    all_results_summary = []

    for k_bits in K_NUM_SEG_BITS_TO_TEST:
        seg_num = 1 << k_bits
        current_config_name = f"kNumSegBits={k_bits} (segNum={seg_num})"
        print(f"\n===== 开始测试配置: {current_config_name} =====")

        build_path = os.path.join(PROJECT_ROOT_DIR, f"{BUILD_DIR_BASE_NAME}_{k_bits}")

        if os.path.exists(build_path):
            print(f"  清理旧的构建目录: {build_path}")
            shutil.rmtree(build_path)
        os.makedirs(build_path)
        print(f"  已创建构建目录: {build_path}")

        cmake_cmd = [
            "cmake",
            f"-DCMAKE_BUILD_TYPE={CMAKE_BUILD_TYPE}",
            f"-DK_NUM_SEG_BITS_FROM_CMAKE={k_bits}", 
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
                print(f"  警告: 测试可执行文件未找到: {test_exe_path}")
                all_results_summary.append({
                    "Config": current_config_name, "Executable": exe_name, 
                    "Status": "EXE_NOT_FOUND", "Metrics": {}
                })
                continue

            print(f"  运行测试: {exe_name}")
            success, stdout_str, stderr_str = run_command([test_exe_path], step_name=f"运行 {exe_name}")
            
            metrics = parse_gtest_output(stdout_str + stderr_str)
            test_status = "PASSED"
          
            if not success or GTEST_FAILURE_INDICATOR in stdout_str or GTEST_FAILURE_INDICATOR in stderr_str:
                test_status = "FAILED"
                print(f"    测试 {exe_name} 失败。输出已记录。")
                
                if success and (GTEST_FAILURE_INDICATOR in stdout_str or GTEST_FAILURE_INDICATOR in stderr_str):
                    success = False # 标记为整体不成功


            all_results_summary.append({
                "Config": current_config_name,
                "Executable": exe_name,
                "Status": test_status,
                "Metrics": metrics,
                "RawOutput": stdout_str + stderr_str if not success else "" # 只记录失败的原始输出
            })
            print(f"    {exe_name} 完成，状态: {test_status}, 指标: {metrics}")

 
    print("\n\n===== 所有测试运行总结 =====")
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
            
            # 打印到控制台
            print(f"配置: {result['Config']}, 可执行文件: {result['Executable']}, 状态: {result['Status']}")
            if result["Status"] == "FAILED":
                print(f"  失败输出:\n{result.get('RawOutput', 'N/A')[:500]}...") # 只打印部分原始输出
            else:
                print(f"  指标: {result['Metrics']}")
            print("-" * 30)
            
    print(f"\n所有结果已保存到 {csv_filename}")
    plot_throughput_comparison(csv_filename)

if __name__ == "__main__":
    main()
