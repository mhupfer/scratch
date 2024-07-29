#/bin/bash

IP=172.31.1.160;
DEST_FOLDER=/data/home/root
USER=root
FILES=()

function print_usage {
  echo "$0 [-h print usage] [-i ip address] [-d destination folder] [-u user] [-p password] file1 [file2 ... file_n]"
  echo Default ip address $IP
  echo Default destination folder $DEST_FOLDER
  echo Default user $USER
}

while [[ $# -gt 0 ]]; do
  case $1 in
    -i|--ip)
      IP="$2"
      shift # past argument
      shift # past value
      ;;
    -d|--destination_folder)
      DEST_FOLDER="$2"
      shift
      shift
      ;;
    -u|--user)
      USER="$2"
      shift
      shift
      ;;
    -p|--password)
      PASSWD="$2"
      shift
      shift
      ;;
    -h|--help)
      print_usage
      exit 0
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      FILES+=($1) 
      shift # past argument
      ;;
  esac
done

if [ "${#FILES[@]}" == 0 ]; then
    echo At least one file is required
    echo
    print_usage
    exit 1
fi

if [ "$PASSWD" == "" ]; then
    PASSWD=$USER
fi

#echo "$IP * $DEST_FOLDER * $USER * $PASSWD"

for FILE in "${FILES[@]}"
do
    echo "$FILE -> $IP:$DEST_FOLDER"
    sshpass -p $PASSWD scp $FILE  ${USER}@${IP}:${DEST_FOLDER}
done
