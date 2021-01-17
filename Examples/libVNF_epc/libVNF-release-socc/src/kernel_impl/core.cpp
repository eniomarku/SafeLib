#include "core.hpp"

/*----------------------------------------------------------------------------*/

struct alignas(16) queue_data {
    //stored in queue Q as length of data required for mtcp_write for char* data
    char *data;
    int data_len;
};
typedef queue<queue_data> Q;
vector<vector<uint16_t>> p_vec; //vector for 10000 ports/thread needed in createClient

int i_count = 0; //index in sock_count map
int s_count = 0; //index inside array of socks corresponding to an index in sock_count
int dummy_connid = 0;
struct timespec tim, tim2;
int temp_sock_for_close;
int listener;//global listner for multicore epoll

struct arg {
    int id;
    int coreno;
    int portnos;
    string ip;
    fn funct_ptr;
};

struct event_arguments {
    int sockid;
    fn funct_ptr;
    int ep;
    int id;
    int coreno;
};

//*******************************Multicore
struct per_core {
    vector<Q> queue_vec;
    unordered_map<int, fn> funct_ptr; //sock_id to handle function
    unordered_map<int, fn> err_funct_ptr; //sock_id to handle error function
    unordered_map<int, uint64_t> conn_map; //client soc_id to server_sockid mapping
    unordered_map<int, uint64_t> data_map;
    unordered_map<int, int> client_list;  //keep a list of accepted client sockids..needed for freeing memory
    unordered_map<int, void *> mem_ptr;      //sockid to mem pool alloc mapping
    unordered_map<void *, int> pkt_ptr;  //map to keep getPktDNE mapping and count
    boost::simple_segregated_storage<std::size_t> storagepkt;  //memory pool for packet
    std::vector<char> mp_pkt_v;  //assuming pkt size 1024 TODO
    boost::simple_segregated_storage<std::size_t> storage1; //pools for request object for 4 different object sizes
    boost::simple_segregated_storage<std::size_t> storage2;
    boost::simple_segregated_storage<std::size_t> storage3;
    boost::simple_segregated_storage<std::size_t> storage4;
    std::unordered_map<int, int>::const_iterator got;  //iterator over client_list
    int memory_size[4]; //to calculate request objects size in power of 2
    int queue_count, j;  //j needed for populate_port
    int conn_counter, pkt_sent, pkt_sent_ds, pkt_recv, pkt_recv_ds; //counters for percore stats
    int ds_sockid, ds_sockid1, set_c, get_c, ds_client_port, ds_client_port1;

    per_core() {
        for (int t1 = 0; t1 < 4; t1++) {
            memory_size[t1] = 0;
        }
        j = 0;
        queue_count = 100000;
        conn_counter = 0;
        pkt_sent = 0;
        pkt_sent_ds = 0;
        pkt_recv = 0;
        pkt_recv_ds = 0;
        ds_sockid = 0;
        ds_sockid1 = 0;
        set_c = 0;
        get_c = 0;
        mp_pkt_v.resize(1024 * 2048); //packet vector size. may have to increase it for very high I/O application
    }

};//__attribute__ ((aligned(64)));




pthread_t *servers;
struct arg *arguments;
struct event_arguments *event_arg;
int *done;
int *ep_arr;
int portno;

string server_ip;

per_core *p_core;

/*----------------------------------------------------------------------------*/
time_t start_app, end_app;
double rate = 0;
double sec = 0;
double packets[7] = {0, 0, 0, 0, 0, 0, 0};
mutex mct, eparr, sock_c, f_ptr_lock, mp_lock, ds_lock, ds_conn_lock, listen_lock;
unordered_map<int, fn> funct_ptr;
boost::simple_segregated_storage<std::size_t> storageds;  //memory pool for data store
std::vector<char> mp_ds(
        1024 * 32768);  //size of memory pool data store. may have to increase this at high load. assuming value size 64
unordered_map<int, void *> ds_map1;   //data store if option is local
unordered_map<void *, int> local_list; //local list of addr for clearing cache..local dnt remove
unordered_map<int, void *> cache_list; //cache list of addr for clearing cache..cache remove
unordered_map<void *, int> cache_void_list; //cache list of addr for clearing cache..cache remove
unordered_map<void *, int> reqptr_list;  //list of addr pointed in req object needed for clearing cache..pointed dnt remove
int ds_size = 0; //to keep count. If exceeds threshold clear
int ds_sizing = 1;

bool useRemoteDataStore = false;
int maxCores;
int bufferSize;
string dataStoreIP;
vector<int> dataStorePorts;
int dataStoreThreshold;
bool isLibvnfInitialized = false;

int initLibvnf(int _maxCores, int _bufferSize, string _dataStoreIP, vector<int> _dataStorePorts,
               int _dataStoreThreshold, bool _useRemoteDataStore) {
    maxCores = _maxCores;
    bufferSize = _bufferSize;
    dataStoreIP = _dataStoreIP;
    dataStorePorts = _dataStorePorts;
    dataStoreThreshold = _dataStoreThreshold;
    isLibvnfInitialized = true;
    useRemoteDataStore = _useRemoteDataStore;

    servers = new pthread_t[maxCores];
    arguments = new struct arg[maxCores];
    event_arg = new struct event_arguments[maxCores];
    done = new int[maxCores];
    ep_arr = new int[maxCores];
    p_core = new per_core[maxCores];
    return 0;
}

/*
This function finds the request block size in power of 2, closest to size specified by the user
new name: initReqPool
*/
void initReqPool(int msize[], int m_tot) {  //size of chunks for request pool and total number of sizes sizeof(msize[])

    int p = 1, i, j;
    cout << "reached here" << endl;
    int temp_memory_size[4];
    if (m_tot > 4) {
        cout << "Only 4 pools allowed" << endl;
        return;  //TODO error handling
    }
    for (i = 0; i < m_tot; i++) {
        p = 1;
        temp_memory_size[i] = 0;
        if (msize[i] && !(msize[i] & (msize[i] - 1))) {
            temp_memory_size[i] = msize[i];
            continue;
        }
        while (p < msize[i])
            p <<= 1;
        temp_memory_size[i] = p;
    }
    cout << "MEMORY_size is " << temp_memory_size[0] << endl;
    for (i = 0; i < maxCores; i++) {
        for (j = 0; j < m_tot; j++) {
            p_core[i].memory_size[j] = temp_memory_size[j];
        }
    }
}

/* Free memory from data store pool when threshold reached. remove cached entry without the dne bit set*/
void free_ds_pool() {

    std::unordered_map<void *, int>::const_iterator gotds;
    for (auto it = cache_void_list.begin(); it != cache_void_list.end(); ++it) {
        gotds = reqptr_list.find(it->first);
        if (gotds == reqptr_list.end()) {
            cache_list.erase(it->second);
            ds_map1.erase(it->second);
            storageds.free(it->first);
        }
    }
    cache_void_list.clear();
    ds_size = 0;
}

void SignalHandler(int signum) {

    //Handle ctrl+C here
    int tot_acc = 0;
    for (int i = 0; i < maxCores; i++) {
        tot_acc += p_core[i].pkt_sent;
    }
    fflush(stdout);
    for (int i = 0; i < maxCores; i++) {
        close(p_core[i].ds_sockid);
        close(p_core[i].ds_sockid1);
        printf("Accpted:%d  %d\n", i, p_core[i].conn_counter);
        printf("recieved :%d   %d\n", i, p_core[i].pkt_recv);
    }
    printf("Accepted Total : %d\n", tot_acc);
    for (int i = 0; i < maxCores; i++) {
        done[i] = 1;
    }
    exit(signum);
}

int read_stream(int conn_fd, uint8_t *buf, int len) {

    //needed for read from DS (redis data store)
    int ptr;
    int retval;
    int read_bytes;
    int remaining_bytes;

    ptr = 0;
    remaining_bytes = len;
    if (conn_fd < 0 || len <= 0) {
        return -1;
    }
    while (1) {
        read_bytes = read(conn_fd, buf + ptr, remaining_bytes);
        if (read_bytes <= 0) {
            retval = read_bytes;
            break;
        }
        ptr += read_bytes;
        remaining_bytes -= read_bytes;
        if (remaining_bytes == 0) {
            retval = len;
            break;
        }
    }
    return retval;
}

//make socket non-blocking
int make_socket_nb(int sfd) {

    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        cout << "Error: NBS fcntl" << '\n';
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        cout << "Error: NBS fcntl flags" << '\n';
        return -1;
    }

    return 0;
}

/*
connect with data store
*/
int
createClientDS(int id, string local_ip, int local_port, string remoteServerIP, int remoteServerPort, string protocol) {

    int ep;
    ep = ep_arr[id];
    int sockid = socket(AF_INET, SOCK_STREAM, 0);
    int ret = -1;
    if (sockid < 0) {
        cout << "Failed to create listening socket!\n";
        return -1;
    }
    ret = make_socket_nb(sockid);
    struct sockaddr_in daddr;

    daddr.sin_family = AF_INET;
    daddr.sin_addr.s_addr = inet_addr((remoteServerIP).c_str());
    daddr.sin_port = htons(remoteServerPort);
    ret = connect(sockid, (struct sockaddr *) &daddr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            perror("mtcp_connect");
            cout << "connect issue" << errno << endl;
            close(sockid);
            return -1;
        }
    }
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = sockid;
    epoll_ctl(ep, EPOLL_CTL_ADD, sockid, &ev);
    return sockid;
}

//Run function on each thread
void serverThreadFunc(void *arg1) {

    //int send_sock;
    cout << "inside server thread func\n";
    int vnf_connid = 0; //connection_id exposed to the vnf code
    struct arg argument = *((struct arg *) arg1);
    int core = argument.coreno;
    int id = argument.id;
    int portnos = argument.portnos;
    fn fn_ptr = argument.funct_ptr;
    void *mm_ptr;
    char *data;
    char *buf2;
    char *temp_x;
    int buf_sockid, buf_key;
    string buf_value;
    int newsockfd = -1, ret;
    queue_data data_to_send;
    cout << "id is " << id << endl;
    /* create epoll descriptor */
    int ep = epoll_create(MAX_EVENTS + 5);
    eparr.lock();
    ep_arr[id] = ep;                                            //handle locking priya
    eparr.unlock();
    if (ep < 0) {
        cout << "Failed to create epoll descriptor!" << endl;
        return NULL;
    }
    /* wait for incoming accept events */
    struct epoll_event ev;
    listen_lock.lock();
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listener;
    epoll_ctl(ep, EPOLL_CTL_ADD, listener, &ev);
    listen_lock.unlock();

    //step 6. epoll_wait
    struct epoll_event *events;
    events = (struct epoll_event *) calloc(MAX_EVENTS, sizeof(struct epoll_event));
    if (!events) {
        cout << "Failed to create event struct!\n";
        exit(-1);
    }
    cout << "Waiting for events" << endl;
    int nevents;
    cout << "started per core" << id << endl;
    bool useRemoteDataStore_l = useRemoteDataStore;
    std::unordered_map<void *, int>::const_iterator got1;
    //memory pool initilization of request objects
    std::vector<char> mp_v1;
    std::vector<char> mp_v2;
    std::vector<char> mp_v3;
    std::vector<char> mp_v4;
    if (p_core[id].memory_size[0] != 0) {
        mp_v1.resize((p_core[id].memory_size[0]) * 2097152);
        cout << "vector size is " << mp_v1.size() << endl;
        p_core[id].storage1.add_block(&mp_v1.front(), mp_v1.size(), p_core[id].memory_size[0]);
    }
    if (p_core[id].memory_size[1] != 0) {
        mp_v2.resize((p_core[id].memory_size[1]) * 2097152);
        cout << "vector size is " << mp_v2.size() << endl;
        p_core[id].storage2.add_block(&mp_v2.front(), mp_v2.size(), p_core[id].memory_size[1]);
    }
    if (p_core[id].memory_size[2] != 0) {
        mp_v3.resize((p_core[id].memory_size[2]) * 2097152);
        cout << "vector size is " << mp_v3.size() << endl;
        p_core[id].storage3.add_block(&mp_v3.front(), mp_v3.size(), p_core[id].memory_size[2]);
    }
    if (p_core[id].memory_size[3] != 0) {
        mp_v4.resize((p_core[id].memory_size[3]) * 2097152);
        cout << "vector size is " << mp_v4.size() << endl;
        p_core[id].storage4.add_block(&mp_v4.front(), mp_v4.size(), p_core[id].memory_size[3]);
    }
    //Packet memory pool
    cout << "vector size for pkt pool is " << p_core[id].mp_pkt_v.size() << endl;
    p_core[id].storagepkt.add_block(&(p_core[id].mp_pkt_v).front(), (p_core[id].mp_pkt_v).size(), 1024);
    cout << "vector size for pkt pool is " << mp_ds.size() << endl;
    ds_lock.lock();
    if (ds_sizing == 1) {
        storageds.add_block(&mp_ds.front(), mp_ds.size(), 256);
        ds_sizing = 0;
    }
    ds_lock.unlock();
    while (!done[id]) {
        if (useRemoteDataStore_l) {
            useRemoteDataStore_l = false;
            //connect to datastore VM
            if (id == 0) {
                p_core[id].ds_sockid = createClientDS(id, argument.ip, p_core[id].ds_client_port, dataStoreIP,
                                                      dataStorePorts[0],
                                                      "tcp");
                p_core[id].ds_sockid1 = createClientDS(id, argument.ip, p_core[id].ds_client_port1, dataStoreIP,
                                                       dataStorePorts[1], "tcp");
                cout << "ds port for " << id << "is " << p_core[id].ds_client_port << " " << p_core[id].ds_client_port1
                     << endl;

            } else {
                p_core[id].ds_sockid = createClientDS(id, argument.ip, p_core[id].ds_client_port, dataStoreIP,
                                                      dataStorePorts[2],
                                                      "tcp");
                p_core[id].ds_sockid1 = createClientDS(id, argument.ip, p_core[id].ds_client_port1, dataStoreIP,
                                                       dataStorePorts[3], "tcp");
                cout << "ds port for " << id << "is " << p_core[id].ds_client_port << " " << p_core[id].ds_client_port1
                     << endl;
            }
        }
        nevents = epoll_wait(ep, events, MAX_EVENTS, -1);
        if (nevents < 0) {
            if (errno != EINTR)
                perror("mtcp_epoll_wait");
            break;
        }

        for (int i = 0; i < nevents; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLRDHUP)
                    ) {
                cout << "ERROR: epoll monitoring failed, closing fd" << '\n';
                if (events[i].data.fd == listener) {
                    cout << "Oh Oh, lsfd it is" << '\n';
                    exit(-1);
                }
                close(events[i].data.fd);
                continue;
            } else if (events[i].data.fd == listener) {
                //Accept connection
                while (1) {
                    newsockfd = accept(listener, NULL, NULL);
                    if (newsockfd < 0) {
                        if ((errno == EAGAIN) ||    //Need lsfd non blocking to run this!!!!!!
                            (errno == EWOULDBLOCK)) {
                            //processed all connections !!!
                            break;
                        } else
                            cout << "Error on accept" << '\n';
                        break;
                    }
                    make_socket_nb(newsockfd);
                    ev.events = EPOLLIN;
                    ev.data.fd = newsockfd;
                    epoll_ctl(ep, EPOLL_CTL_ADD, newsockfd, &ev);
                    p_core[id].mem_ptr[newsockfd] = NULL;
                    p_core[id].conn_counter++;
                    p_core[id].funct_ptr[newsockfd] = argument.funct_ptr; //callback function initialised

                }
            } else if (events[i].events & EPOLLIN) {
                ret = 1;
                if (events[i].data.fd == p_core[id].ds_sockid || events[i].data.fd == p_core[id].ds_sockid1) {
                    //reply from data store
                    while (1) {
                        DSPacketHandler pkt;
                        int pkt_len, retval;
                        pkt.clear_pkt();
                        retval = read_stream(events[i].data.fd, pkt.data, sizeof(int));
                        if (retval < 0) {
                            if (errno == EAGAIN) {
                                break;
                            }
                        } else {
                            p_core[id].pkt_recv_ds++;
                            memmove(&pkt_len, pkt.data, sizeof(int) * sizeof(uint8_t));
                            pkt.clear_pkt();
                            retval = read_stream(events[i].data.fd, pkt.data, pkt_len);
                            pkt.data_ptr = 0;
                            pkt.len = retval;
                            if (retval < 0) {
                                cout << "Error: Packet from HSS Corrupt, break" << endl;
                                break;
                            }
                        }
                        pkt.extract_item(buf_sockid);
                        pkt.extract_item(buf_key);
                        pkt.extract_item(buf_value);
                        buf_value = buf_value + '\0';
                        void *getds;
                        ds_lock.lock();
                        if (ds_size == dataStoreThreshold) {
                            free_ds_pool();
                        }
                        //cache the state
                        getds = storageds.malloc();
                        ds_size++;
                        memcpy(getds, buf_value.c_str(), buf_value.length());
                        ds_map1[buf_key] = getds;
                        cache_list[buf_key] = getds;
                        cache_void_list[getds] = buf_key;
                        ds_lock.unlock();
                        fn_ptr = p_core[id].funct_ptr[buf_sockid];
                        vnf_connid = id * 10000000 + buf_sockid;
                        fn_ptr(vnf_connid, p_core[id].mem_ptr[buf_sockid], buf_value.c_str(), bufferSize, 0);
                    }
                } else {
                    //packet from othe VNFs/ client
                    data = static_cast<char *>(p_core[id].storagepkt.malloc());//put the read data in packet pool
                    int rd = read(events[i].data.fd, data, bufferSize);
                    if (rd <= 0) {
                        temp_sock_for_close = events[i].data.fd;
                        int close_ret = close(temp_sock_for_close);
                        if (close_ret < 0)
                            cout << "connection not close" << endl;
                        p_core[id].storagepkt.free(static_cast<void *>(data));
                        continue;
                    }
                    p_core[id].pkt_recv++;
                    fn_ptr = p_core[id].funct_ptr[events[i].data.fd];
                    vnf_connid = id * 10000000 + events[i].data.fd;
                    fn_ptr(vnf_connid, p_core[id].mem_ptr[events[i].data.fd], data, bufferSize,
                           0); //callback function called
                    got1 = p_core[id].pkt_ptr.find(static_cast<void *>(data));
                    if (got1 == p_core[id].pkt_ptr.end()) {
                        p_core[id].storagepkt.free(static_cast<void *>(data)); //free if DNE bit unset
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                //connect done. send the queued up packets
                while (!(p_core[id].queue_vec[events[i].data.fd]).empty()) {
                    data_to_send = (p_core[id].queue_vec[events[i].data.fd]).front();
                    ret = write(events[i].data.fd, data_to_send.data, data_to_send.data_len);
                    p_core[id].storagepkt.free((void *) data_to_send.data);
                    (p_core[id].queue_vec[events[i].data.fd]).pop();
                    if (ret < 0) {
                        cout << "Connection closed with client." << endl;
                        close(events[i].data.fd);
                        break;
                    }
                }
            }

        }
    }
    cout << "exiting while loop" << endl;


}

//set up IP port number of server socket
int createServer(string inter_face, string server_ip1, int server_port, string protocol) {
    assert(isLibvnfInitialized);
    portno = server_port;
    server_ip = server_ip1;
    dummy_connid++;
    return dummy_connid;
}

/*allocate a memory block for request object
allocReqObj
*/
void *allocReqObj(int vnf_connid, int index) {

    int id, alloc_sockid;
    id = vnf_connid / 10000000;
    alloc_sockid = vnf_connid % (10000000);
    p_core[id].client_list[alloc_sockid] = alloc_sockid;
    if (index == 1) {
        p_core[id].mem_ptr[alloc_sockid] = static_cast<void *>(p_core[id].storage1.malloc());    //lock TODO
    } else if (index == 2) {
        p_core[id].mem_ptr[alloc_sockid] = static_cast<void *>(p_core[id].storage2.malloc());    //lock TODO
    } else if (index == 3) {
        p_core[id].mem_ptr[alloc_sockid] = static_cast<void *>(p_core[id].storage3.malloc());    //lock TODO
    } else if (index == 4) {
        p_core[id].mem_ptr[alloc_sockid] = static_cast<void *>(p_core[id].storage4.malloc());    //lock TODO
    }
    if (p_core[id].mem_ptr[alloc_sockid] == 0) {
        cout << "could not malloc" << endl;
    }
    return p_core[id].mem_ptr[alloc_sockid];

}

/*
free up memory of request object
freeReqObj
*/
void freeReqObj(int vnf_connid, int index) {

    int id, alloc_sockid;
    id = vnf_connid / 10000000;
    alloc_sockid = vnf_connid % (10000000);
    p_core[id].got = p_core[id].client_list.find(alloc_sockid);
    if (p_core[id].got == p_core[id].client_list.end()) {
        p_core[id].mem_ptr.erase(alloc_sockid);
    } else {
        if (index == 1) {
            p_core[id].storage1.free(static_cast<void *>(p_core[id].mem_ptr[alloc_sockid]));
        } else if (index == 2) {
            p_core[id].storage2.free(static_cast<void *>(p_core[id].mem_ptr[alloc_sockid]));
        } else if (index == 3) {
            p_core[id].storage3.free(static_cast<void *>(p_core[id].mem_ptr[alloc_sockid]));
        } else if (index == 4) {
            p_core[id].storage4.free(static_cast<void *>(p_core[id].mem_ptr[alloc_sockid]));
        }
        p_core[id].mem_ptr.erase(alloc_sockid);
        p_core[id].client_list.erase(alloc_sockid);
    }
}

/*
get pointer to already allocated request object
linkReqObj
*/
void linkReqObj(int vnf_connid, void *requestObj) {

    int id, connID;
    id = vnf_connid / 10000000;
    connID = vnf_connid % (10000000);
    p_core[id].mem_ptr[connID] = requestObj;
}

/*
get pointer to packet
setPktDNE
*/
void *getPktDNE(int vnf_connid, void *pkt_mem_ptr) {

    int id;
    id = vnf_connid / 10000000;
    p_core[id].pkt_ptr[pkt_mem_ptr] = 1;
    return pkt_mem_ptr;
}

/*
remove pointer to packet
unsetPktDNE
*/
void unsetPktDNE(int vnf_connid, void *pkt_mem_ptr) {

    int id;
    id = vnf_connid / 10000000;
    p_core[id].storagepkt.free(pkt_mem_ptr);
    p_core[id].pkt_ptr.erase(pkt_mem_ptr);
}

/*
pointer to key value pair
setKeyDNE
*/
void *setKeyDNE(int ds_key) {

    void *temp_ds;
    ds_lock.lock();
    reqptr_list[ds_map1[ds_key]] = ds_key;
    temp_ds = ds_map1[ds_key];
    ds_lock.unlock();
    return temp_ds;
}

/*
remove pointer to key value pair
unsetKeyDNE
*/
void unsetKeyDNE(int ds_key) {

    ds_lock.lock();
    reqptr_list.erase(ds_map1[ds_key]);
    ds_lock.unlock();
    return;

}

/*
get memory from packet pool
getPktBuf
*/
char *getPktBuf(int vnf_connid) {

    int id;
    id = vnf_connid / 10000000;
    char *pkt = static_cast<char *>(p_core[id].storagepkt.malloc());
    return pkt;
}

/*
starts the per core thread function. creates the global listen socket
*/
void startEventLoop() {

    int ret = -1;
    for (int j = 0; j < maxCores; j++) {
        for (int i = 0; i < p_core[j].queue_count; i++) {
            p_core[j].queue_vec.push_back(Q());
        }
    }
    cout << "reached here" << endl;
    /* register signal handler*/
    signal(SIGINT, SignalHandler);
    cout << "Application initialization finished.\n";
    for (int i = 0; i < maxCores; i++) {
        done[i] = 0;
    }
    string c_ip = "127.0.0.1";
    p_vec.resize(maxCores);
    //create global listener socket
    listener = socket(AF_INET, SOCK_STREAM, 0);
    sock_c.lock();
    cout << "s_coun is " << s_count << endl;
    sock_c.unlock();
    //unlock
    if (listener < 0) {
        cout << "Failed to create listening socket!" << endl;
        return -1;
    }
    ret = make_socket_nb(listener);
    if (ret < 0) {
        cout << "Failed to set socket in nonblocking mode." << endl;
        return -1;
    }
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr((server_ip).c_str());//inet_addr("192.168.100.2");//INADDR_ANY;
    saddr.sin_port = htons(portno);

    ret = bind(listener, (struct sockaddr *) &saddr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        cout << "Failed to bind to the listening socket!\n";
        return -1;
    }

    /* listen (backlog: 4K) */
    ret = listen(listener, 4096);
    if (ret < 0) {
        cout << "mtcp_listen() failed!\n";
        return -1;
    }
    cout << "listen socket created\n";

    //spawn server threads
    for (int i = 0; i < maxCores; i++) {
        arguments[i].coreno = i;
        arguments[i].id = i;
        arguments[i].ip = server_ip;
        arguments[i].funct_ptr = funct_ptr[dummy_connid];
        arguments[i].portnos = portno;
        if (i == (maxCores - 1))
            funct_ptr.clear();  //TODO start client only after all threads have started or it would clear actual sockid mappings
        pthread_create(&servers[i], NULL, serverThreadFunc, &arguments[i]);
    }
    //Wait for server threads to complete
    for (int i = 0; i < maxCores; i++) {
        pthread_join(servers[i], NULL);
    }
    int send_sock = i_count;
    i_count++;
    return send_sock;
}

/*
registers a callback with an event on the socket
*/
void registerCallback(int vnf_connid, enum event_type t2, void callbackFnPtr(int, void *, char *, int, int)) {

    int id, connID;
    id = vnf_connid / 10000000;
    if (vnf_connid != 1) {
        connID = vnf_connid % (10000000);
        if (t2 != ERROR) {
            p_core[id].funct_ptr[connID] = callbackFnPtr;
        } else {
            p_core[id].err_funct_ptr[connID] = callbackFnPtr;
        }
    } else
        funct_ptr[vnf_connid] = callbackFnPtr;

}

/*
connect as a client to another VNF
*/
int createClient(int vnf_connid, string local_ip, string remoteServerIP, int remoteServerPort, string protocol) {

    int ep, id;
    id = vnf_connid / 10000000;
    ep = ep_arr[id];
    int sockid = socket(AF_INET, SOCK_STREAM, 0);
    int ret = -1;
    if (sockid < 0) {
        cout << "Failed to create listening socket!" << errno << endl;
        return -1;
    }
    ret = make_socket_nb(sockid);
    if (ret < 0) {
        cout << "Failed to set socket in nonblocking mode." << endl;
        return -1;
    }
    struct sockaddr_in daddr;

    daddr.sin_family = AF_INET;
    daddr.sin_addr.s_addr = inet_addr((remoteServerIP).c_str());
    daddr.sin_port = htons(remoteServerPort);
    ret = connect(sockid, (struct sockaddr *) &daddr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            perror("mtcp_connect");
            cout << "connect issue" << errno << endl;
            close(sockid);
            return -1;
        }
    }
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT;// | EPOLLET ;
    ev.data.fd = sockid;
    epoll_ctl(ep, EPOLL_CTL_ADD, sockid, &ev);
    int dummy_vnf_id = id * 10000000 + sockid;
    return dummy_vnf_id;
}

/*
send data. buffer packets in queue if connection not established
*/
void sendData(int vnf_connid, char *packetToSend, int packet_size) {

    int id, connID;
    id = vnf_connid / 10000000;
    connID = vnf_connid % (10000000);
    if (connID == -1)
        cout << "connID issue" << endl;
    if (!p_core[id].queue_vec[connID].empty()) {
        queue_data data_to_send;
        data_to_send.data = packetToSend;
        data_to_send.data_len = packet_size;
        p_core[id].queue_vec[connID].push(data_to_send);
    } else {  //for c
        int ret = 1, my_errno;
        ret = write(connID, packetToSend, packet_size);
        my_errno = errno;
        if (ret < 0) {
            if (my_errno == EAGAIN) {
                queue_data data_to_send;
                data_to_send.data = packetToSend;
                data_to_send.data_len = packet_size;
                p_core[id].queue_vec[connID].push(data_to_send);
            } //for c
            else
                cout << "error on write" << endl;
        } else if (ret != packet_size) {
            cout << "packet complete not sent" << endl;
            queue_data data_to_send;
            data_to_send.data = packetToSend;
            data_to_send.data_len = packet_size;
            p_core[id].queue_vec[connID].push(data_to_send);

        } else {
            if (connID == p_core[id].ds_sockid || connID == p_core[id].ds_sockid1)
                p_core[id].pkt_sent_ds++;
            else
                p_core[id].storagepkt.free((void *) packetToSend);
            p_core[id].pkt_sent++;
        }
    }  //for c
}

/*
set data in data store. if remote option then cache and then send to data store VM
If local alloc a block in data store memory pool and store the value
*/
void setData(int vnf_connid, string table_name, int key, enum data_location location, void *value, int value_len,
             void callbackFnPtr(int, void *, void *, int, int)) {
    if (callbackFnPtr != NULL)
        registerCallback(vnf_connid, ERROR, callbackFnPtr);

    int id, connID, ds_conn_id;
    id = vnf_connid / 10000000;
    connID = vnf_connid % (10000000);
    char *s2;
    if (location == REMOTE) {
        value = value + '\0';
        ds_lock.lock();
        if (ds_size == dataStoreThreshold) {
            free_ds_pool();
        }
        void *setds = storageds.malloc();
        ds_size++;
        memcpy(setds, value, value_len);
        //memcpy(setds,value.c_str(),value.length());
        ds_map1[key] = setds;
        cache_list[key] = setds;
        cache_void_list[setds] = key;
        ds_lock.unlock();
        string snd_cmd = "set", snd_table = "abc", snd_value = "xyz";
        int snd_sockid = 5, snd_key = 10;
        s2 = (char *) (value);
        std::string s3(s2);
        DSPacketHandler pkt;
        pkt.clear_pkt();
        pkt.append_item(connID);
        pkt.append_item(snd_cmd);
        pkt.append_item(snd_table);
        pkt.append_item(key);
        pkt.append_item(s3);
        pkt.prepend_len();
        if (p_core[id].set_c == 0) {
            ds_conn_id = id * 10000000 + p_core[id].ds_sockid;
            sendData(ds_conn_id, pkt.data, pkt.len);
            p_core[id].set_c = 1;
        } else {
            ds_conn_id = id * 10000000 + p_core[id].ds_sockid1;
            sendData(ds_conn_id, pkt.data, pkt.len);
            p_core[id].set_c = 0;
        }
    } else {
        //usleep(20); // rdma emulate
        /*value = value + '\0';
        void* setds;
        ds_lock.lock();
        if(ds_size==dataStoreThreshold){
            free_ds_pool();
        }
        setds = storageds.malloc();
        ds_size++;
        memcpy(setds,value.c_str(),value.length());
        ds_map1[key] = setds;
        local_list[setds] = key;
        ds_lock.unlock();
        */
        void *setds;
        std::unordered_map<int, void *>::const_iterator got_set;
        ds_lock.lock();
        if (ds_size == dataStoreThreshold) {
            free_ds_pool();
        }
        got_set = ds_map1.find(key);
        if (got_set == ds_map1.end()) {
            setds = storageds.malloc();
            ds_size++;
        } else {
            setds = ds_map1[key];
        }
        memcpy(setds, value, value_len);
        ds_map1[key] = setds;
        local_list[setds] = key;
        ds_lock.unlock();
    }
}

/*
retreive data from data store. if checkcache option retrieve from cache if entry exists else fetch from remote store
store the callback to be called after value retreived
*/
void getData(int vnf_connid, string table_name, int key, enum data_location location,
             void callbackFnPtr(int, void *, void *, int, int)) {
    int id, connID, ds_conn_id;
    id = vnf_connid / 10000000;
    connID = vnf_connid % (10000000);
    if (location == CHECKCACHE) {
        registerCallback(vnf_connid, READ, callbackFnPtr);
        int cache_check = 0;
        std::unordered_map<int, void *>::const_iterator got_ds;
        ds_lock.lock();
        got_ds = cache_list.find(key);
        if (got_ds != cache_list.end()) {
            cache_check = 1;
        }
        ds_lock.unlock();
        if (cache_check == 1) {
            cache_check = 0;
            fn fn_ptr;
            char *ds_value;
            ds_value = static_cast<char *>(cache_list[key]);
            fn_ptr = p_core[id].funct_ptr[connID];
            fn_ptr(vnf_connid, p_core[id].mem_ptr[connID], ds_value, bufferSize, 0);
        } else {
            string snd_cmd = "get", snd_table = "abc", snd_value = "xyz";
            int snd_sockid = 5, snd_key = 10;
            DSPacketHandler pkt1;
            pkt1.clear_pkt();
            snd_cmd = "get";
            pkt1.append_item(connID);
            pkt1.append_item(snd_cmd);
            pkt1.append_item(snd_table);
            pkt1.append_item(key);
            pkt1.prepend_len();
            if (p_core[id].get_c == 0) {
                ds_conn_id = id * 10000000 + p_core[id].ds_sockid;
                sendData(ds_conn_id, pkt1.data, pkt1.len);
                p_core[id].get_c = 1;
            } else {
                ds_conn_id = id * 10000000 + p_core[id].ds_sockid1;
                sendData(ds_conn_id, pkt1.data, pkt1.len);
                p_core[id].get_c = 0;
            }
        }
    } else if (location == REMOTE) {
        registerCallback(vnf_connid, READ, callbackFnPtr);
        string snd_cmd = "get", snd_table = "abc", snd_value = "xyz";
        int snd_sockid = 5, snd_key = 10;
        DSPacketHandler pkt1;
        pkt1.clear_pkt();
        snd_cmd = "get";
        pkt1.append_item(connID);
        pkt1.append_item(snd_cmd);
        pkt1.append_item(snd_table);
        pkt1.append_item(key);
        pkt1.prepend_len();
        if (p_core[id].get_c == 0) {
            ds_conn_id = id * 10000000 + p_core[id].ds_sockid;
            sendData(ds_conn_id, pkt1.data, pkt1.len);
            p_core[id].get_c = 1;
        } else {
            ds_conn_id = id * 10000000 + p_core[id].ds_sockid1;
            sendData(ds_conn_id, pkt1.data, pkt1.len);
            p_core[id].get_c = 0;
        }
    } else {
        //usleep(10);
        /* fn fn_ptr;
         char* ds_value;
         ds_lock.lock();
         ds_value = static_cast<char*>(ds_map1[key]);
         ds_lock.unlock();
         fn_ptr = p_core[id].funct_ptr[connID];
         */
        void *ds_value;
        ds_lock.lock();
        ds_value = ds_map1[key];
        ds_lock.unlock();
        callbackFnPtr(vnf_connid, p_core[id].mem_ptr[connID], ds_value, bufferSize, 0);
    }
}

/*
delete data from data store
*/
void delData(int vnf_connid, string table_name, int key, enum data_location location) {

    int id, connID;
    id = vnf_connid / 10000000;
    connID = vnf_connid % (10000000);
    if (location == REMOTE) {
        std::unordered_map<int, void *>::const_iterator gotds;
        ds_lock.lock();
        gotds = cache_list.find(key);
        ds_lock.unlock();
        if (gotds != cache_list.end()) {
            ds_lock.lock();
            cache_void_list.erase(cache_list[key]);
            storageds.free(cache_list[key]);
            cache_list.erase(key);
            ds_map1.erase(key);
            ds_lock.unlock();
        }
    } else {
        void *temp_ds;
        ds_lock.lock();
        temp_ds = ds_map1[key];
        local_list.erase(temp_ds);
        storageds.free(ds_map1[key]);
        ds_map1.erase(key);
        ds_lock.unlock();
    }
}

void closeConn(int vnf_connid) {

    int id, connID;
    id = vnf_connid / 10000000;
    connID = vnf_connid % (10000000);
    close(connID);
}
