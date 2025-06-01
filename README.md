# MyLRU
My concurrent LRU cache implementation.

## Usage
```bash
chmod +x build.sh
./build.sh
```
## Test
```bash
python basic_test.py
python seg_test.py
```
`basic_test`: There are five configuration combinations in basic_test, which will build an executable program according to the configuration and run it 10 times, calculating the average throughput, hit rate and running time.
`seg_test`: In the main function, you can modify the number of TEST_CONFIGURATIONS to change the running configuration. It will build the corresponding executable program according to the configuration to test the throughput of different numbers of segments and draw a line chart.
