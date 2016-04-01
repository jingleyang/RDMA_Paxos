#!/usr/bin/env python
# -*- coding: utf-8 -*-
#python2.7.x

import ConfigParser
import argparse
import logging
import sys
import os


def processOptions(options):
    for option in options:
        if option.endswith('HOOK'):
            hook_option = option
        elif option.endswith('CHECKPOINT'):
            checkpoint_option = option
        elif option.endswith('COMPARE'):
            compare_option = option
        else:
            logging.critical("strange option " + option)
    return hook_option,checkpoint_option,compare_option

#def enableServer(config,bench):
#    server_program=config.get(bench,'SERVER_PROGRAM') 
#    server_input=config.get(bench,'SERVER_INPUT')



def processBench(config, bench):
    logger.debug("processing: " + bench)
    eval_options = config.get(bench,'EVAL_OPTIONS').split(',')
    hook_option,check_option,compare_option = processOptions(eval_options)
    #print eval_options
    #print hook_option + check_option + compare_option
    server_count=config.getint(bench,'SERVER_COUNT')
    server_program=config.get(bench,'SERVER_PROGRAM')
    if len(server_program)!=0
        
    server_input=config.get(bench,'SERVER_INPUT')
    server_kill=config.get(bench,'SERVER_KILL')
    client_program=config.get(bench,'CLIENT_PROGRAM')
    client_input=config.get(bench,'CLIENT_INPUT')
    os.system(server_program + " " + server_input)
    os.system(client_program + " " + client_input)
    os.system(server_kill)




def readConfigFile(config_file):
	try:
		newConfig = ConfigParser.ConfigParser()
		ret = newConfig.read(config_file)
	except ConfigParser.MissingSectionHeaderError as e:
		logging.error(str(e))
	except ConfigParser.ParsingError as e:
		logging.error(str(e))
	except ConfigParser.Error as e:
		logging.critical("strange error? " + str(e))
	else:
		if ret:
			return newConfig

def getConfigFullPath(config_file):
    try:
        with open(config_file) as f:
            pass
    except IOError as e:
        logging.warning("'%s' does not exist" % config_file)
        return None
    return os.path.abspath(config_file)



if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG,
                        format='%(asctime)s %(name)-s %(levelname)-s %(message)s',
                        datefmt='%m-%d %H:%M',
                        filename='/tmp/myapp.log',
                        filemode='w')
    # define a Handler which writes INFO messages or higher to the sys.stderr
    console = logging.StreamHandler()
    console.setLevel(logging.DEBUG)
    # set a format which is simpler for console use
    formatter = logging.Formatter('%(name)-s: %(levelname)-s %(message)s')
    # tell the handler to use this format
    console.setFormatter(formatter)
    # add the handler to the root logger
    logging.getLogger('').addHandler(console)

    logger = logging.getLogger()

    #see if there is an environment variable
    try:
        RDMA_ROOT = os.environ["RDMA_ROOT"]
        logger.debug('RDMA_ROOT = ' + RDMA_ROOT)
    except KeyError as e:
        logger.error("Please set the environment variable " + str(e))
        sys.exit(1)
    
    try:
        local_host = os.popen('cat $RDMA_ROOT/apps/env/local_host').readline()
        local_host_ip = local_host[local_host.find("@")+1:]
        remote_hostone = os.popen('cat $RDMA_ROOT/apps/env/remote_host1').readline()
        remote_hostone_ip = remote_hostone[remote_hostone.find("@")+1:]
        remote_hosttwo = os.popen('cat $RDMA_ROOT/apps/env/remote_host2').readline()
        remote_hosttwo_ip = remote_hosttwo[remote_hosttwo.find("@")+1:]
    except KeyError as e:
        logger.error("Please set the host file " + str(e))
        sys.exit(1)

    #find the location of bash and print it
    bash_path = os.popen("which bash").readlines()
    #print bash_path
    if not bash_path:
        logging.critical("cannot find shell 'bash'")
        sys.exit(1)
    else:
        bash_path = bash_path[0]
        logging.debug("find 'bash' at " + bash_path)

    #parse args and give some help how to handle this file
    parser = argparse.ArgumentParser(description='Apps Evaluation')
    parser.add_argument('--filename','-f',nargs='*',
                        type=str,
                        default=["default.cfg"],
                        help="list of configuration files")
    args = parser.parse_args()

    if args.filename.__len__() == 0:
        logging.critical(' no configuration file specified ')
        sys.exit(1)
    elif args.filename.__len__() == 1:
        logging.debug('config file: ' + ''.join(args.filename))
    else:
        logging.debug('config files: ' + ', '.join(args.filename))

    for config_file in args.filename:
        logging.info("processing '" + config_file + "'")
        full_path = getConfigFullPath(config_file)

        local_config = readConfigFile(full_path)
        if not local_config:
            logging.warning("skip " + full_path)
            continue

        benchmarks = local_config.sections()
        for benchmark in benchmarks:
            if benchmark == "default":
                continue
            processBench(local_config, benchmark)




