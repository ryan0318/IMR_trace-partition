# IMR_trace-partition
Turning CMR drive trace files into IMR-like drive trace files

IMR_trace(partition version)

How to run
1.configure parameters in IMR_trace.h:

-LBATOTAL: number of logical blocks

-TRACK_NUM: number of disk's tracks

-SECTORS_PER_BOTTRACK & SECTORS_PER_TOPTRACK: average top/bottom's track size (in sectors)

-SEGMENT_SIZE: size of a segment (in sectors)

-TOTAL_BOTTRACK & TOTAL_TOPTRACK: number of top/bottom tracks

-HOTDATA_MAXSIZE: it's hot data if the access size is smaller than HOTDATA_MAXSIZE (in sectors)

-PARTITION_SIZE: default size of a partition (in tracks)

-MAX_PARTITION_SIZE: maximum size of a partition (in tracks)

-BUFFER_SIZE: number of buffer tracks in a partition (in tracks)

-MAPPING_CACHE_SIZE: size of the mapping cache (in partitions)

2. compile .cpp then run program with arguments
   example: IMR_trace.exe in.trace out.trace
   argument 1: source trace file 
   argument 2: output file name

