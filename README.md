# PARALLEL-FILE-FINDER

### Operating Systems Course Assignment
C program which probes a directory tree for files whose name matches user-provided search term. The program parallelizes its work using threads and a Taylor-made shared synchronized queue

Command line arguments:

• argv[1]: search root directory (search for files within this directory and its subdirectories).

• argv[2]: search term (search for file names that include the search term).

• argv[3]: number of searching threads to be used for the search (assume a valid integer greater
than 0)

The program prints all the files that contain the search-term



