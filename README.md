# UServer

### HTTP Server written in C

It responds to requests using the next methods:

    1. First In First Out (FIFO).
    2. Fork (Generate a new heavy process for each request).
    3. Threaded (Generate a new thread for each request).
    4. Pre-Forked (Pool of k heavy processes ready to serve).
    5. Pre-Threaded (Pool of k threads ready to serve).