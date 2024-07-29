#/bin/bash

IP=172.31.1.160;
PORT=8000
ARCH=x86_64
ARGUMENTS=""

while [[ $# -gt 0 ]]; do
  case $1 in
    -i|--ip)
      IP="$2"
      shift # past argument
      shift # past value
      ;;
    -p|--port)
      PORT="$2"
      shift
      shift
      ;;
    -a|--architecture)
      ARCH="$2"
      shift
      shift
      ;;
    -b|--breakpoint)
      BREAKPOINT="$2"
      shift
      shift
      ;;
    -s|--stage)
      STAGE_DIR="$2"
      shift
      shift
      ;;
    -h|--help)
      echo "$0 [-h print usage] [-i ip addess] [-p port] [-A architecture] [-s staging directory] [-b breakpoint] file [arguments]"
      exit 0
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      if [ "$PATH_TO_FILE" == "" ]; then
        PATH_TO_FILE=$1
      else
        ARGUMENTS=$@
        break
      fi

      shift # past argument
      ;;
  esac
done

if [ "$PATH_TO_FILE" == "" ]; then
    echo File name is required
    exit 1
fi

FILENAME=$(basename $PATH_TO_FILE)
INIT_FILE_NAME=$FILENAME.gdb.init

#create gdb.init
echo "target qnx $IP:$PORT" > $INIT_FILE_NAME
echo "file $PATH_TO_FILE" >> $INIT_FILE_NAME
echo "upload $PATH_TO_FILE /tmp/$FILENAME" >> $INIT_FILE_NAME
echo "set args $ARGUMENTS" >> $INIT_FILE_NAME

if [ "$STAGE_DIR" != "" ]; then
    echo "set solib-search-path $STAGE_DIR"  >> $INIT_FILE_NAME
fi

if [ "$BREAKPOINT" != "" ]; then
    echo "b $BREAKPOINT"  >> $INIT_FILE_NAME
fi

#launch gdb
exec nto${ARCH}-gdb -x $INIT_FILE_NAME

