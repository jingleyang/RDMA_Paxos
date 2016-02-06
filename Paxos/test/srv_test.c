void usage(char *prog)
{
    printf("Usage: %s [OPTIONS]\n"
            "OPTIONS:\n"
            "\t--start | --join                         (Server type; default start)\n"
            "\t-n <hostname>                            (server's name)\n"
            "\t-s <size>                                (group size; default 3)\n"
            "\t-i <index>                               (server's index)\n"
            "\t-l <log>                                 (log file)\n.",
            prog);
}
int main(int argc, char* argv[])
{
    int rc; 
    char *log_file="";
    server_input_t input = {
        .log = stdout,
        .name = "",
        .srv_type = SRV_TYPE_START,
        .group_size = 3,
        .server_idx = 0xFF
    };
    int c;
    static int srv_type = SRV_TYPE_START;
    static int sm_type = CLT_KVS;
    
    while (1) {
        static struct option long_options[] = {
            /* These options set the type of the server */
            {"start", no_argument, &srv_type, SRV_TYPE_START},
            {"join",  no_argument, &srv_type, SRV_TYPE_JOIN},
            /* Other options */
            {"help", no_argument, 0, 'h'},
            {"hostname", required_argument, 0, 'n'},
            {"size", required_argument, 0, 's'},
            {"index", required_argument, 0, 'i'},
            {"log", required_argument, 0, 'l'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        c = getopt_long (argc, argv, "hn:s:i:l:",
                       long_options, &option_index);
        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                printf("option %s", long_options[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf("\n");
                break;

            case 'h':
                usage(argv[0]);
                exit(1);
                
            case 'n':
                input.name = optarg;
                break;
                
            case 's':
                input.group_size = (uint8_t)atoi(optarg);
                break;
                
            case 'i':
                input.server_idx = (uint8_t)atoi(optarg);
                break;
            
            case 'l':
                log_file = optarg;
                break;

            case '?':
                break;

            default:
                exit(1);
        }
    }
    input.srv_type = srv_type;

    if (strcmp(log_file, "") != 0) {
        input.log = fopen(log_file, "w+");
        if (NULL == input.log) {
            printf("Cannot open log file\n");
            exit(1);
        }
    }
    if (SRV_TYPE_START == input.srv_type) {
        if (0xFF == input.server_idx) {
            printf("A server cannot start without an index\n");
            usage(argv[0]);
            exit(1);
        }
    }
    
    rc = server_init(&input);
    if (0 != rc) {
        fprintf(log_fp, "Cannot init client\n");
        return 1;
    }
    
    fclose(log_fp);
    
    return 0;
}
