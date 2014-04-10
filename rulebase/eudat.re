################################################################################
#                                                                              #
# EUDAT Safe-Replication and PID management policies                           #
#                                                                              #
################################################################################

################################################################################
#                                                                              #
# Site specific functions                                                      #
#                                                                              #
################################################################################

getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug) {
    *credStoreType="os";
    *credStorePath="/srv/irods/current/modules/BE2SAFE/cmd/credentials_test";
    *epicApi="http://hdl.handle.net/";
    *serverID="irods://<hostnameWithFullDomain>:1247"; 
    *epicDebug=2; 

    getEUDATAuthZ("$userNameClient#$rodsZoneClient", "read", *credStorePath, *response);
}

getAuthZParameters(*authZMapPath) {
    *authZMapPath="/srv/irods/current/modules/BE2SAFE/cmd/authz.map.json";
}

getLogParameters(*logConfPath) {
    *logConfPath="/srv/irods/current/modules/BE2SAFE/cmd/log.manager.conf";
}

################################################################################
#                                                                              #
# Utility functions                                                            #
#                                                                              #
################################################################################

#
#It manages the writing and reading of log messages to/from external log services.
#
#Return
# no response is expected
#
# Parameters:
#   *message        [IN]    the message to be logged
#   *level          [IN]    the logging level 
#
# Author: Claudio Cacciari, Cineca
#
EUDATLog(*message, *level) {
    getLogParameters(*logConfPath);
    logInfo("logging message '*message'");
    msiExecCmd("log.manager.py", "*logConfPath log *level *message",
               "null", "null", "null", *out);
}

#
#It implements a FIFO queue for messages to/from external log services.
#
#Return
# no response is expected for action "push"
# The first message of the queue for action "pop"
#
# Parameters:
#   *action         [IN]    the queueing action, which the user would like to perform
#                           [ push | pop ]
#   *message        [IN/OUT]the message to be queued or read.
#                           It can be a comma separated list of strings, each between single apices.
#   *number         [IN]    the number of elements to be extracted
#
# Author: Claudio Cacciari, Cineca
#
EUDATQueue(*action, *message, *number) {
    getLogParameters(*logConfPath);
    *options = "";
    if (*action == 'pop' || *action == 'queuesize') {
        *message = "";
    }
    if (*action == 'pop' && *number > 1) {
        *options = "-n "++str(*number);
    }
    logInfo("logging action '*action' for message '*message'");
    msiExecCmd("log.manager.py", "*logConfPath *action *options *message",
               "null", "null", "null", *out);
    if (*action == 'pop' || *action == 'queuesize') {
        msiGetStdoutInExecCmdOut(*out, *message);
    }
    if (*action == 'pop' && *number > 1) {
        *message = triml(*message, "[");
        *message = trimr(*message, "]");
    }
}

#
# Return a boolean value:
#   True, if the authorization request matches against, at least
#   one assertion listed in the authz.map.json file
#   False otherwise.
#
# Parameters:
#   *user           [IN]    a username, related to the user who request a permission
#   *action         [IN]    the action, which the user would like to perform
#   *target         [IN]    the target of the action
#   *response       [OUT]   True or False depending on authorization rights
#
# Author: Claudio Cacciari, Cineca
#
getEUDATAuthZ(*user, *action, *target, *response) {
    getAuthZParameters(*authZMapPath);
    logInfo("checking authorization for *user to perform: *action *target");
    msiExecCmd("authZ.manager.py", "*authZMapPath check *user '*action' '*target'",
               "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *response);
    if (*response == "False") {
        # here should be placed specific authorization rules 
        # EUDATsetFilterACL(*action, *target, null, null, *status);
        # if (*status == "false") {}
        logInfo("authorization denied");
        msiExit("-1", "user is not allowed to perform the requested action");
    }
    else {
        # here should be placed specific authorization rules 
        # EUDATsetFilterACL(*action, *target, null, null, *status);
        # if (*status == "true") {}
        logInfo("authorization granted");
    }
}

#
# Return the absolute path to the iRODS collection where all command files are stored.
#   typically "<zone>/replicate". Make sure all users and remote users have write permissions here.
#
# Parameters:
#   *zonePath           [IN]    a iRODS path name including the zone
#   *collectionPath     [OUT]   the iRODS absolute path to the collection used to store command files
#
# Author: Willem Elbers, MPI-TLA
#
getSharedCollection(*zonePath, *collectionPath) {
    msiGetZoneNameFromPath(*zonePath, *zoneName);
    *collectionPath = "*zoneName/replicate/";
}

#
# Write a command file
#
# Parameters:
#   *file       [IN]    iRODS location of the file to write
#   *contents   [IN]    the command contents
#
# Author: Willem Elbers, MPI-TLA
#
writeFile(*file, *contents) {
    msiDataObjCreate("*file", "forceFlag=", *filePointer);
    msiDataObjWrite(*filePointer, "*contents", *bytesWritten);
    msiDataObjClose(*filePointer, *outStatus);
}

#
# Read a command file
#
# Parameters:
#   *file       [IN]    iRODS location of the file to read
#   *contents   [OUT]   the command contents
#
# Author: Willem Elbers, MPI-TLA
#
readFile(*file, *contents) {
    msiDataObjOpen("objPath=*file++++replNum=0++++openFlags=O_RDONLY",*S_FD);
    msiDataObjRead(*S_FD,"1024",*R_BUF);
    #msiDataObjRead(*S_FD,null,*R_BUF);
    msiBytesBufToStr(*R_BUF, *contents);
    msiDataObjClose(*S_FD,*closeStatus);
}

#
# Rename a command file.
# Appends the current timestamp and a ".success" or ."failed" identifier 
#
# Parameters:
#   *cmdPath    [IN]    the command file to rename
#   *status     [OUT]   status, 0 = ok, <0 = error
#
# Author: Willem Elbers, MPI-TLA
#
updateCommandName(*cmdPath, *status) {
    msiGetFormattedSystemTime(*ftime,"human","%d%02d%02dT%02d%02d%02d");
    if(*status == 0) {
        msiDataObjRename(*cmdPath,"*cmdPath.*ftime.success","0",*renameStatus);
    } else {
        msiDataObjRename(*cmdPath,"*cmdPath.*ftime.failed","0",*renameStatus);
    }
}

#
# Logging policies
#

logInfo(*msg) {
    logWithLevel("info", *msg);
}

logDebug(*msg) {
#    logWithLevel("debug", *msg);
# replaced "debug" with "info" to print even without
# changing the log level of iRODS
     logWithLevel("info", *msg);
}

logError(*msg) {
    logWithLevel("error", *msg);
}

logWithLevel(*level, *msg) {
    msiWriteToLog(*level,"*msg");
}

#
# Monitor the specified pid command file
#
# Parameters:
#   *file   [IN]    start a monitor on the specified iRODS file
#
# Author: Willem Elbers, MPI-TLA
#
updateMonitor(*file) {
    logInfo("updateMonitor(*file)");
    delay("<PLUSET>1m</PLUSET>") {
        if(errorcode(msiObjStat(*file,*out)) >= 0) {
            logInfo("*file exists");
            processPIDCommandFile(*file);
        } else {
            logInfo("*file does not exist yet");
            # save *source of failed_transfered data object into fail_log 
            processErrorUpdatePID(*file);
        }
    }
}

#
# Retrieve the checksum for an object stored in iRODS
# And compute it using msiDataObjChksum if the object does not have any yet
#
# Parameters:
#   *path       [IN]    the path of the object in iRODS
#   *checksum   [OUT]   the checksum retrieved from the iCAT
#
# Author: Willem Elbers, MPI-TLA, edited by Elena Erastova, RZG
#
retrieveChecksum(*path, *checksum) {
    *debugFlag = bool("false");
    *checksum = -1;
    msiSplitPath(*path,*parent,*child);
    msiExecStrCondQuery("SELECT DATA_CHECKSUM WHERE COLL_NAME = '*parent' AND DATA_NAME = '*child'" ,*B);
    foreach ( *B ) {
        msiGetValByKey(*B,"DATA_CHECKSUM", *checksum);
        if (debugFlag == bool("true")) {
            logInfo("[retrieveChecksum] stored checksum: *checksum");
        }
    }
    #compute checksum using msiDataObjChksum if the object does not have any yet
    if (*checksum == "") {
        msiDataObjChksum(*path, "null", *checksum);
        if (debugFlag == bool("true")) {
            logInfo("[retrieveChecksum] computed checksum: *checksum");
        }
    } 
}

################################################################################
#                                                                              #
# Command file triggers                                                        #
#                                                                              #
################################################################################

#
# Start a replication by writing a .replicate command file
#
# Parameters:
#   *commandFile    [IN]    the absolute filename to store the command in
#   *pid            [IN]    pid of the digital object
#   *source         [IN]    source path of the object to replicate
#   *destination    [IN]    destination path of the object to replicate
#
# Author: Willem Elbers, MPI-TLA
#
triggerReplication(*commandFile,*pid,*source,*destination) {
    logInfo("startReplication(*commandFile,*pid,*source,*destination)");
    getRorPid(*pid, *ror);
    if (*ror == "None"){
        getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
        *ror = "*epicApi"++"*pid";  
    }
    writeFile("*commandFile","*pid;*source;*destination;*ror");
}

#
# Start a PID created by writing a .pid.create command file
#
# Parameters:
#   *commandFile    [IN]    the absolute filename to store the command in
#   *pid            [IN]    PID of the digital object
#   *destination    [IN]    destination path of the object to replicate
#   *ror            [IN]    ROR of the original digital object
#
# Author: Willem Elbers, MPI-TLA
#
triggerCreatePID(*commandFile,*pid,*destination,*ror) {
    logInfo("triggerCreatePID(*commandFile,*pid,*destination,*ror)");
    writeFile("*commandFile", "create;*pid;*destination;*ror");
}

#
# Author: Willem Elbers, MPI-TLA
#
# Parameters:
#   *commandFile    [IN]    the absolute filename to store the command in
#   *pid            [IN]    PID of the digital object
#   *new_pid        [IN]    place of the replicated digital object.
#
triggerUpdateParentPID(*commandFile,*pid,*new_pid) {
    logInfo("triggerUpdateParentPID(*commandFile,*pid,*new_pid)");
    writeFile("*commandFile", "update;*pid;*new_pid");
}

################################################################################
#                                                                              #
# Process command files                                                        #
#                                                                              #
################################################################################

#
# Process a .replicate file and perform the replication
# format = "command1,command2,command2,..."
#
# command format = "source_pid;source_path;destination_path"
#
# Parameters:
#   *cmdPath    [IN]    the path to the .replicate file
#
# Author: Willem Elbers, MPI-TLA
#
processReplicationCommandFile(*cmdPath) {
    logInfo("processReplication(*cmdPath)");

    readFile(*cmdPath, *out_STRING);    

    #TODO: properly manage status here
    *status = 0;    
    *out_ARRAY = split(*out_STRING, "\n");
    foreach(*out_STRING1 in *out_ARRAY) {
        *list = split(*out_STRING1, ";");
        # assign values from array to parameters
        *ror = "None";
        *counter=0;
        foreach (*item_LIST in *list) {
            if      ( *counter == 0 ) { *pid         = *item_LIST ; }
            else if ( *counter == 1 ) { *source      = *item_LIST ; }
            else if ( *counter == 2 ) { *destination = *item_LIST ; }
            else if ( *counter == 3 ) { *ror         = *item_LIST ; }
            *counter = *counter + 1;    
        }
        *list_size = *counter ;

        if ((*list_size==4) || (*list_size==3)){
            doReplication(*pid,*source,*destination,*ror,*status);
        }
        else {
            logError("ignoring incorrect command: [*out_STRING]");
            *status = -1;
        }
    }
    updateCommandName(*cmdPath,*status);
}

#
# Read a .replicate file and perform the replication
# format = "command1,command2,command2,..."
#
# command format = "source_pid;source_path;destination_path"
#
# Parameters:
#   *cmdPath    [IN]    the path to the .replicate file
#
# Author: Willem Elbers, MPI-TLA
# Edited: Elena Erastova, RZG
#
readReplicationCommandFile(*cmdPath,*pid,*source,*destination,*ror) {
    readFile(*cmdPath, *out_STRING);
    *out_ARRAY = split(*out_STRING, "\n");
    foreach(*out_STRING1 in *out_ARRAY) {
        *list = split(*out_STRING1, ";");
        # assign values from array to parameters
        *ror = "None";
        *counter=0;
        foreach (*item_LIST in *list) {
            if      ( *counter == 0 ) { *pid         = *item_LIST ; }
            else if ( *counter == 1 ) { *source      = *item_LIST ; }
            else if ( *counter == 2 ) { *destination = *item_LIST ; }
            else if ( *counter == 3 ) { *ror         = *item_LIST ; }
            *counter = *counter + 1;
        }
    }
}

#
# Process a .pid file and perform the appropriate action
#   supported actions: create, update
#
# Parameters:
#   *cmdPath    [IN]    the iRODS path to the pid command file
#
# Author: Willem Elbers, MPI-TLA
# Edited: Elena Erastova, RZG
#
processPIDCommandFile(*cmdPath) {
        logInfo("processPID(*cmdPath)");
        readFile(*cmdPath, *out_STRING);
        *list = split(*out_STRING, ";");

    # assign values from array to parameters
    *ror = "None";
    *parent = "None";
    *counter=0;
    foreach (*item_LIST in *list) {
        if      ( *counter == 0 ) { *pidAction   = *item_LIST ; }
        else if ( *counter == 1 ) { *parent      = *item_LIST ; }
        else if ( *counter == 2 ) { *destination = *item_LIST ; }
        else if ( *counter == 3 ) { *ror         = *item_LIST ; }
        *counter = *counter + 1;
    }
    *list_size = *counter ;

    # process command/action
    if ((*list_size==4) || (*list_size==3)){
        if(*pidAction == "create") {
            #manage pid in this repository
            createPID(*parent, *destination, *ror, *new_pid, bool("true"));
            getSharedCollection(*destination,*collectionPath);
            #create .pid.update file based on absolute file path
            msiReplaceSlash(*destination,*filepathslash);
            triggerUpdateParentPID("*collectionPath*filepathslash.pid.update", *parent, *new_pid);
        }
        else if(*pidAction=="update") {
            *status = 0;
            updatePIDWithNewChild(*parent, *destination);
            updateCommandName(*cmdPath,*status);
        }
        else {
            logError("ignoring incorrect command: [*out_STRING]");
        }
    }
    else {
        logError("ignoring incorrect list");
    }
}

#
# Start a replication
#
# Parameters:
#   *pid            [IN]    pid of the digital object
#   *source         [IN]    source path of the object to replicate
#   *destination    [IN]    destination path of the object to replicate
#   *ror            [IN]    ROR of the digital object
#   *status         [OUT]   status, 0 = ok, <0 = error
#
# Author: Willem Elbers, MPI-TLA
#
doReplication(*pid, *source, *destination, *ror, *status) {
    logInfo("doReplication(*pid, *source, *destination, *ror)");

    #make sure the parent collections exist
    msiSplitPath(*destination, *parent, *child);
    msiCollCreate(*parent, "1", *collCreateStatus);

    #rsync object (make sure to supply "null" if dest resource should be the default one) 
    msiDataObjRsync(*source, "IRODS_TO_IRODS", "null", *destination, *rsyncStatus);

    if(*pid != "null") {
        #trigger pid management in destination
        getSharedCollection(*destination,*collectionPath);
        # create .pid.create file and monitor for .pid.update based on absolute file path
        msiReplaceSlash(*destination,*filepathslash);
        triggerCreatePID("*collectionPath*filepathslash.pid.create", *pid, *destination, *ror);
        updateMonitor("*collectionPath*filepathslash.pid.update");
    }
    else {
        logInfo("No pid management");
    }
}

################################################################################
#                                                                              #
# Hooks to the EPIC API, depend on wrapper scripts around the epicclient.py    #
# script                                                                       #
#                                                                              #
################################################################################

# Legend (the prefix i stands for iCAT and e for epic):
#
# ePID PID on the EPIC server
# iPID PID record in the iCAT
# eCHECKSUM checksum on the EPIC server
# iCHECKSUM checksum record on the iCAT

# List of the function:
#
# createPID(*parent_pid, *path, *ror, *newPID, *iCATCache)
# addPIDWithChecksum(*path, *newPID)
# searchPID(*path, *existing_pid)
# searchPIDchecksum(*path, *existing_pid)
# CheckReplicas(*source, *destination, *commandFile)
# updatePIDWithNewChild(*parentPID, *childPID)
# getRorPid(*pid, *ror)
# EUDATeiPIDeiChecksumMgmt(*ePIDcheck, *iCATuse, *minTime)
# EUDATiPIDcreate(*path, *PID)
# EUDATiCHECKSUMretrieve(*path, *checksum)
# EUDATiCHECKSUMget(*path, *checksum)
# EUDATiPIDretrieve(*path, *PID)
# EUDATiFieldVALUEretrieve(*path, *FNAME, *FVALUE)
# EUDATePIDcreate(*path, *PID)
# EUDATePIDsearch(*field, *value, *PID)
# EUDATeCHECKSUMupdate(*PID)
# EUDATeURLupdate(*PID, *newURL)
# EUDATePIDremove(*location)
# EUDATgetObjectTimeDiff(*filePath, *age)
# EUDATfileInPath(*path,*subColl)

#
# Generate a new PID for a digital object.
# Fields stored in the PID record: URL, ROR and CHECKSUM
# adds a ROR field if (*ror != "None")
#
# Parameters:
#   *parent_pid [IN]    the PID of the digital object that was replicated to us (not necessarily the ROR)
#   *path       [IN]    the path of the replica to store with the PID record
#   *ror        [IN]    the ROR PID (absolute url) of the digital object that we want to store.
#   *iCATCache  [IN]    the boolean flag to enable creation of PID in the iCAT 
#   *newPID     [OUT]   the pid generated for this replica 
#
# Author: Willem Elbers, MPI-TLA
# Edited by Elena Erastova, RZG
#
createPID(*parent_pid, *path, *ror, *newPID, *iCATCache) {
    logInfo("create pid for *path and save *ror as ror");
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);

    #check if PID already exists
    searchPID(*path, *existing_pid);

    if((*existing_pid == "empty") || (*existing_pid == "None")) {
        # create PID
        if(*epicDebug > 1) {
            logDebug("epicclient.py *credStoreType *credStorePath create *serverID*path");
        }
        msiExecCmd("epicclient.py", "*credStoreType *credStorePath create *serverID*path", "null", "null", "null", *out);
        msiGetStdoutInExecCmdOut(*out, *newPID);
	if(*epicDebug > 1) {
            logDebug("created handle = *newPID");
	}

        if (*iCATCache == bool("true")) {
            # Add PID into iCAT
            logInfo("Begin to SAVE PID into field AVU -PID- of iCAT *path with PID = *newPID");
            EUDATiPIDcreate(*path, *newPID);
            #msiSetReplComment("null",*path,0,*newPID);
            logInfo("---> PID's saved in iCAT with AVU PID");
        }

        # add CHECKSUM to PID record
        retrieveChecksum(*path, *checksum) ::: msiDataObjChksum(*path, "null", *checksum);
        if(*epicDebug > 1) {
            logDebug("epicclient.py *credStoreType *credStorePath modify *newPID CHECKSUM *checksum");
        }
        msiExecCmd("epicclient.py","*credStoreType *credStorePath modify *newPID CHECKSUM *checksum", "null", "null", "null", *out3);
        msiGetStdoutInExecCmdOut(*out3, *response3);
	if(*epicDebug > 1) {
            logDebug("modify handle response = *response3");
	}
    }
    else {
        *newPID = *existing_pid;
        logInfo("PID already exists (*newPID)");
    }
    # add RoR to PID record if there is one defined
    *myRor = *ror;
    *myPpid = *parent_pid;
    if(*myRor != "None") {
        *listRor = split(*myRor, "/");
        *firstRor = elem(*listRor,0);
        if(*firstRor != "http:") {
            *myRor = *epicApi*myRor;
        }
    }
    if(*myPpid != "None") {
        *listPpid = split(*myPpid, "/");
        *firstPpid = elem(*listPpid,0);
        if(*firstPpid != "http:") {
            *myPpid = "*epicApi*myPpid";
        }
        if(*myRor == "None") {
            *myRor = *myPpid;
            *myPpid = "None";   
        }
        else if(*myRor == *myPpid) {
            *myPpid = "None";
        }
    }
    if(*myRor != "None") {
        # add RoR to PID record
        if(*epicDebug > 1) {
            logDebug("epicclient.py *credStoreType *credStorePath modify *newPID ROR *myRor");
        }
        msiExecCmd("epicclient.py","*credStoreType *credStorePath modify *newPID EUDAT/ROR *myRor", "null", "null", "null", *out4);
        msiGetStdoutInExecCmdOut(*out4, *response4);
        if(*epicDebug > 1) {
            logDebug("modify handle response = *response4")
	}
    }
    if(*myPpid != "None") {
        # add PPID to PID record
        if(*epicDebug > 1) {
            logDebug("epicclient.py *credStoreType *credStorePath modify *newPID EUDAT/PPID *myPpid");
        }
        msiExecCmd("epicclient.py","*credStoreType *credStorePath modify *newPID EUDAT/PPID *myPpid", "null", "null", "null", *out44);
        msiGetStdoutInExecCmdOut(*out44, *response44);
        if(*epicDebug > 1) {
            logDebug("modify handle response = *response44")
	}
    }
}

#
# Parameters:
#   *path       [IN]    the path of the replica to store with the PID record
#   *newPID     [OUT]   the pid generated for this replica 
#
# Author: CINECA, edited and added by Elena Erastova, RZG
#
addPIDWithChecksum(*path, *newPID) {
	createPID("None", *path, "None", *newPID, bool("false"));
}

#
# searchPID searching fo a PID using URL replacing "#", "%" and "&" with "*"
# (uses replaceHash in epicclient.py)
# Parameters:
#   *path       	[IN]    the path of the replica
#   *existing_pid	[OUT]   existing PID 
#
# Author: Elena Erastova, RZG
#
searchPID(*path, *existing_pid) {
    logInfo("search pid for *path");
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
    #check if PID already exists
    if(*epicDebug > 1) {
        logDebug("epicclient.py *credStoreType *credStorePath search URL *serverID*path");
    }
    msiExecCmd("epicclient.py","*credStoreType *credStorePath replaceHash *path", "null", "null", "null", *out1);
    msiGetStdoutInExecCmdOut(*out1, *path1);
    msiExecCmd("epicclient.py", "*credStoreType *credStorePath search URL *serverID*path1", "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *existing_pid);
}

#
# searchPIDROR searching fo a PID using EUDAT/ROR
# Parameters:
#   *ror       		[IN]    the ROR of the replica
#   *existing_pid	[OUT]   existing PID 
#
# Author: Elena Erastova, RZG
#
searchPIDROR(*ror, *existing_pid) {
    logInfo("search pid for *ror");
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
    #check if PID already exists
    if(*epicDebug > 1) {
        logDebug("epicclient.py *credStoreType *credStorePath search EUDAT/ROR *ror");
    }
    msiExecCmd("epicclient.py", "*credStoreType *credStorePath search EUDAT/ROR *ror", "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *existing_pid);
}

#
# searchPIDchecksum searching fo a PID using CHECKSUM
# Parameters:
#   *path       	[IN]    the path of the replica
#   *existing_pid	[OUT]   existing PID 
#
# Author: Elena Erastova, RZG
#
searchPIDchecksum(*path, *existing_pid) {
    logInfo("search pid for *path");
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
    #check if PID already exists
    #msiDataObjChksum(*path, "null", *checksum);

    *checksum = "";
    msiSplitPath(*path,*parent,*child);
    msiExecStrCondQuery("SELECT DATA_CHECKSUM WHERE COLL_NAME = '*parent' AND DATA_NAME = '*child'" ,*B);
    foreach   ( *B )    {
        msiGetValByKey(*B,"DATA_CHECKSUM", *checksum);
    }
    logInfo("search by CHECKSUM inside = *checksum");
    
    if(*checksum == "") {
        *existing_pid ="empty";
        logInfo("search by CHECKSUM inside if no checksum = *checksum");
    }
    else {
        msiExecCmd("epicclient.py", "*credStoreType *credStorePath search CHECKSUM *checksum", "null", "null", "null", *out);
        msiGetStdoutInExecCmdOut(*out, *existing_pid);
        logInfo("existing_pid = *existing_pid");
        msiExecCmd("epicclient.py", "*credStoreType *credStorePath read *existing_pid --key URL", "null", "null", "null", *out);
        msiGetStdoutInExecCmdOut(*out, *URL);
        msiSplitPath(*URL,*parent1,*child1);
        if("*serverID*parent" != *parent1) {
            *existing_pid ="empty";
            logInfo("parent  = *serverID*parent ; parent1 = *parent1");
        }

        logInfo("search by CHECKSUM inside call search = *existing_pid");
    }
}

#
# Check whether two files are available and identical
# If they are not identical, do the following:
#    1. find the pid of the object and modify checksum in the pid or create a new pid
#    2. create pid in the iCAT if it does not exist
#    3. add/update ROR in iCAT
#    4. tregger replication from source to destination
#
# Parameters:
#   *source         [IN]     source of the file
#   *destination    [IN]     destination of the file
# Author: Elena Erastova, RZG
#
checkReplicas(*source, *destination) {
    logInfo("Check if 2 replicas have the same checksum. Source = *source, destination = *destination");
    if (catchErrorChecksum(*source, *destination) == bool("false") || catchErrorSize(*source, *destination) == bool("false")) {
        EUDATeiPIDeiChecksumMgmt2(*source, *pid);
        EUDATiRORupdate2(*source, *pid);
        logInfo("replication from *source to *destination");
        getSharedCollection(*source,*collectionPath);
        msiReplaceSlash(*destination,*filepathslash);
        triggerReplication("*collectionPath*filepathslash.replicate",*pid,*source,*destination);
    }
}

#
# Update a PID record with a new child.
#
# Parameters:
#       *parentPID  [IN]    PID of the record that will be updated
#       *childPID   [IN]    PID to store as one of the child locations
#
# Author: Willem Elbers, MPI-TLA
# Modified by: Claudio Cacciari, CINECA
#
updatePIDWithNewChild(*parentPID, *childPID) {
    logInfo("update parent pid (*parentPID) with new child (*childPID)");
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
    if(*epicDebug > 1) {
        logDebug("epicclient.py *credStoreType *credStorePath relation *parentPID *epicApi*childPID");
    }
    msiExecCmd("epicclient.py","*credStoreType *credStorePath relation *parentPID *epicApi*childPID", "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *response);
    if(*epicDebug > 1) {
    	logDebug("update handle location response = *response");
    }
}

#
# get the ROR entry for a PID
#
# Parameters:
#   *pid    [IN]     PID that you want to get the ROR for
#   *ror    [OUT]    ROR for *pid
#
getRorPid(*pid, *ror) {
    logInfo("get RoR from (*pid)");
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
    msiExecCmd("epicclient.py", "*credStoreType *credStorePath read *pid --key EUDAT/ROR", "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *ror);
    if(*ror=="None") {
        msiExecCmd("epicclient.py", "*credStoreType *credStorePath read *pid --key ROR", "null", "null", "null", *out);
        msiGetStdoutInExecCmdOut(*out, *ror);
        if(*ror=="None") {
            msiWriteRodsLog("getRorPID -> NO ROR for *pid ", *status);
        }
    }
}

#
# This function create a PID for $objPath and store its value and the checksum in the iCAT if it does ot exist.
# Otherwhise the function modify the PID.
#
# Environment variable used:
#   $objPath
#
# Arguments:
#   *ePIDcheck       [IN]    Specify wheter you want to search for ePID (bool("true")) or not
#   *iCATuse         [IN]    Specify wheter you want to use the iCAT (bool("true")) or not
#   *minTime         [IN]    Specify the minimum age of the digital before looking for ePID
#
# Author: Giacomo Mariani, CINECA
#
EUDATeiPIDeiChecksumMgmt(*ePIDcheck, *iCATuse, *minTime) {
    msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> Look if the PID is in the iCAT", *status);
    # Search for iPID and, if it exists, enter the if below
    if (EUDATiFieldVALUEretrieve($objPath, "PID", *oldPID)) 
    {
        msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> Update PID with CHECKSUM for: *oldPID, $userNameClient, $objPath", *status);
        EUDATeCHECKSUMupdate(*oldPID);                           # Update the eCHECKSUM.
    }
    # iPID does not exist
    else
    {
        # If *ePIDcheck look for ePID
        *oldPID = "empty";
        if ((*ePIDcheck) && (*iCATuse)) {
            msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> No PID registered in iCAT. Looking on the EPIC server.", *status);
            EUDATgetObjectTimeDiff($objPath, *liveTime);
            # If the file is older than a day look for ePID
            if ( *liveTime >= *minTime ) then
            {
                getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug)
                # Get ePID looking for one between: URL and CHECKSUM.
                #msiDataObjChksum($objPath, "null", *checksum);
                EUDATePIDsearch("URL", "*serverID"++"$objPath", *oldPID);
            }
        }
        # If ePID does not exist create both ePID and iPID
        if ( *oldPID == "empty" ) then
            { 
            msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> No PID in epic server yet", *status);
            EUDATePIDcreate($objPath, *newPID);                  # Create ePID
            if (*iCATuse) {EUDATiPIDcreate($objPath, *newPID)};            # Create iPID
        }
        else
        {
            msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> Modifying the PID in epic server: *oldPID", *status);  
            EUDATeCHECKSUMupdate(*oldPID);                       # Update eCHECKSUM
            if (*iCATuse) {EUDATiPIDcreate($objPath, *oldPID)};            # Create iPID
        }
    }
}

#
# The function write iPID given ePID.
#
# Environment variable used:
#   $objPath
#
# Arguments:
#   *PID             [IN] ePID
#
# Author: Giacomo Mariani, CINECA
#
EUDATiPIDcreate(*path, *PID) {
    msiAddKeyVal(*Keyval,"PID", "*PID");
    writeKeyValPairs('serverLog', *Keyval, " is : ");
    msiGetObjType(*path, *objType);
    msiAssociateKeyValuePairsToObj(*Keyval, *path, *objType);
    msiWriteRodsLog("EUDATiPIDcreate -> Added PID = *PID to metadata of *path", *status);
}

#
# The function retrieve iCHECKSUM for a given object.
#
# Environment variable used:
#
# Arguments:
#   *path               [IN]    the iRODS path of the object involved in the query
#   *checksum           [OUT]   iCHECKSUM
#   *status             [REI]   false if no value is found, trou elsewhere
#
# Author: Giacomo Mariani, CINECA
#
EUDATiCHECKSUMretrieve(*path, *checksum) {
    *status = bool("false");
    msiWriteRodsLog("EUDATiCHECKSUMretrieve -> Looking at *path", *status1);
    msiSplitPath(*path, *coll, *name);
    *d = SELECT DATA_CHECKSUM WHERE DATA_NAME = '*name' AND COLL_NAME = '*coll'; 
    foreach(*c in *d) {
        msiGetValByKey(*c, "DATA_CHECKSUM", *checksum);
        if (*checksum==""){
            writeLine("serverLog","EUDATiCHECKSUMretrieve -> The iCHECKSUM is empty");
        }else{
            writeLine("serverLog","EUDATiCHECKSUMretrieve -> Found iCHECKSUM = *checksum");
            *status = bool("true");
        }
    }
    *status;
}

#
# The function obtain iCHECKSUM for a given object creating it if necessary.
#
# Environment variable used:
#
# Arguments:
#   *path               [IN]    the iRODS path of the object involved in the query
#   *checksum           [OUT]   iCHECKSUM
#
# Author: Giacomo Mariani, CINECA
#
EUDATiCHECKSUMget(*path, *checksum) {
    if (!EUDATiCHECKSUMretrieve(*objPath, *checksum)) {
        msiDataObjChksum(*path, "null", *checksum);
    }
}


#
# The function retrieve iPID. 
#
# Environment variable used:
#   $objPath
#
# Arguments:
#   *PID             [IN]    iPID
#   *status          [REI]   false if no value is found, trou elsewhere
#
# Author: Giacomo Mariani, CINECA
#
EUDATiPIDretrieve(*path, *PID) {
    *status = bool("false");
    msiSplitPath(*path, *coll, *name);
    writeLine ("serverLog","EUDATiPIDretrieve -> path = *path , coll = *coll , name = *name");
    *d = SELECT META_DATA_ATTR_VALUE WHERE DATA_NAME = '*name' AND COLL_NAME = '*coll' AND META_DATA_ATTR_NAME = 'PID'; 
    foreach(*c in *d) {
        msiGetValByKey(*c, "META_DATA_ATTR_VALUE", *PID);
        writeLine("serverLog","EUDATiPIDretrieve -> PID = *PID");
        *status = bool("true");
    }
    *status;
}

#
# The function retrieves the value of the required field.
#
# Environment variable used:
#
# Arguments:
#   *path               [IN]    the iRODS path of the object involved in the query
#   *FNAME              [IN]    the name of the iCAT field the function is going to look for
#   *FVALUE             [OUT]   the corresponding value, if any
#   *status0            [REI]   false if no value is found, trou elsewhere
#
# Author: Giacomo Mariani, CINECA
#
EUDATiFieldVALUEretrieve(*path, *FNAME, *FVALUE) {
    msiWriteRodsLog("EUDATiFieldVALUEretrieve -> looking for *FNAME of *path", *status);
    *status0 = bool("false");
    msiSplitPath(*path, *coll, *name);
    *d = SELECT META_DATA_ATTR_VALUE WHERE DATA_NAME = '*name' AND COLL_NAME = '*coll' AND META_DATA_ATTR_NAME = '*FNAME'; 
    foreach(*c in *d) {
        msiGetValByKey(*c, "META_DATA_ATTR_VALUE", *FVALUE);
        msiWriteRodsLog("EUDATiFieldVALUEretrieve -> *FNAME equal to= *FVALUE", *status);
        *status0 = bool("true");
    }
    *status0;
}

#
# The function create ePID.
#
# Environment variable used:
#
# Arguments:
#   *path            [IN]   The full iRODS path of the object
#   *PID             [OUT]    The created ePID.
#
# Author: Giacomo Mariani, CINECA
#
EUDATePIDcreate(*path, *PID) {
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug) ;
    msiWriteRodsLog("EUDATePIDcreate -> Add PID with CHECKSUM to: USER, OBJPATH: $userNameClient, *path", *status);
    msiDataObjChksum(*path, "null", *checksum);
    msiWriteRodsLog("EUDATePIDcreate -> The CHECKSUM is: *checksum", *status);
    msiExecCmd("epicclient.py","*credStoreType *credStorePath create *serverID"++"*path --checksum *checksum", "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *PID);
    msiWriteRodsLog("EUDATePIDcreate -> Created handle is: *PID", *status);
}

#
# The function retrieve ePID searching for a field between URL, CHECKSUM. 
#
# Environment variable used:
#   $objPath
#
# Arguments:
#   *field           [IN]    The eField to look in
#   *value           [IN]    The valou to search for
#   *PID             [OUT]   ePID
#   *status0         [REI]   false if no value is found, trou elsewhere
#
# Author: Giacomo Mariani, CINECA
#
EUDATePIDsearch(*field, *value, *PID) {
    msiWriteRodsLog("EUDATePIDsearch -> search the PID with *field equal to *value", *status);
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
    *status0 = bool("true");
    msiExecCmd("epicclient.py","*credStoreType *credStorePath search *field \"*value\"", "null", "null", "null", *out);    
    msiGetStdoutInExecCmdOut(*out, *PID);
    msiWriteRodsLog("EUDATePIDsearch -> search handle response = *PID", *status);
    if ( str(*PID) == "empty" ) { 
        *status0=bool("false"); 
        msiWriteRodsLog("EUDATePIDsearch -> search handle response = no PID", *status);
    }
    *status0;
}

#
# This function update the checksum field of the PID of $objPath
#
# Environment variable used:
#   $objPath        
#
# Arguments:
#   *PID                [IN] The PID associated to $objPath
#
# Author: Giacomo Mariani, CINECA
#
EUDATeCHECKSUMupdate(*PID) {
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug) 
    msiWriteRodsLog("EUDATeCHECKSUMupdate -> modify checksum related to PID *PID", *status);
    msiDataObjChksum($objPath, "null", *checksum);
    msiWriteRodsLog("EUDATeCHECKSUMupdate -> created checksum = *checksum", *status);
    msiExecCmd("epicclient.py","*credStoreType *credStorePath modify *PID CHECKSUM *checksum", "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *response);
    msiWriteRodsLog("EUDATeCHECKSUMupdate -> modify handle response = *response", *status);
}
#
# This function update the checksum field of the PID of the object
#
# Arguments:
#   *PID                [IN] The PID associated to $objPath
#   *path               [IN] Object path
#
# Author: Giacomo Mariani, CINECA
# Added: Elena Erastova, RZG
#
EUDATeCHECKSUMupdate2(*PID,*path) {
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
    msiWriteRodsLog("EUDATeCHECKSUMupdate -> modify checksum related to PID *PID", *status);
    msiDataObjChksum(*path, "null", *checksum);
    msiWriteRodsLog("EUDATeCHECKSUMupdate -> created checksum = *checksum", *status);
    msiExecCmd("epicclient.py","*credStoreType *credStorePath modify *PID CHECKSUM *checksum", "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *response);
    msiWriteRodsLog("EUDATeCHECKSUMupdate -> modify handle response = *response", *status);
}

#
# This function update the URL field of the PID of $objPath
#
# Environment variable used:
#   $objPath        
#
# Arguments:
#   *PID                [IN] The PID associated to $objPath
#   *newURL             [IN] The new URL to be associated to the PID of $objPath
#
# Author: Giacomo Mariani, CINECA
#
EUDATeURLupdate(*PID, *newURL) {
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug) ;
    msiWriteRodsLog("EUDATeCHECKSUMupdate -> modify URL in PID *PID", *status);
    msiExecCmd("epicclient.py","*credStoreType *credStorePath modify *PID URL \"*newURL\"", "null", "null", "null", *out);
    # WARNING: the following line clean all the 10320/LOC field instead of removing only the old entry... 
    #          This is good since it is the only one for EPOS. To be improved. 
    msiExecCmd("epicclient.py","*credStoreType *credStorePath modify *PID 10320/LOC ''", "null", "null", "null", *out);
    msiGetStdoutInExecCmdOut(*out, *response);
    msiWriteRodsLog("EUDATeCHECKSUMupdate -> modify handle response = *response", *status);
}


#
# This function remove an ePID... even if its 10320/loc field is not empty!
# To be improved.
#
# Environment variable used:
#   $objPath        
#
# Arguments:
#   *location           [IN]    The location associated to $objPath in ePID
#
# Author: Giacomo Mariani, CINECA
#
EUDATePIDremove(*location) {
    getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug) 
    msiWriteRodsLog("EUDATePIDremove -> Removing PID associated to: $userNameClient, $objPath ($filePath) \"*location\"", *status);
    if (EUDATePIDsearch("URL", "*location", *pid)){
      msiExecCmd("epicclient.py","*credStoreType *credStorePath read --key 10320/LOC *pid", "null", "null", "null", *out2);
      msiGetStdoutInExecCmdOut(*out2, *loc10320);
      msiWriteRodsLog("EUDATePIDremove -> get 10320/LOC from handle response = *loc10320", *status);
#      if ("*loc10320" like "Error*")||("*loc10320" == "")||("*loc10320" like "None*") then {
        msiWriteRodsLog("EUDATePIDremove -> 10320/LOC does not exist or is empty: PID will be deleted", *status);
#            msiExecCmd("deleteHandle.py","*pid","null", "null", "null", *out3);
            msiExecCmd("epicclient.py","*credStoreType *credStorePath delete *pid", "null", "null", "null", *out3);
            msiGetStdoutInExecCmdOut(*out3, *response3);
            msiWriteRodsLog("EUDATePIDremove -> delete handle response = *response3", *status);
        # The PID record could be associated to a replica.
        # The field 10320/LOC of the parent PID record should be updated
#   }
#   else {
#       # The PID record contains pointers to other DO copies.
#       # What should we do?
#       # Maybe all the copies should be deleted together with the master copy.
#       msiWriteRodsLog("EUDATePIDremove -> The PID record *pid contains pointers to other DO copies", *status);
#   }
    }else{
       msiWriteRodsLog("EUDATePIDremove -> No PID associated to *location found", *status);
    }
}

#
# Calculate the difference between the creation time and the modification time of an object.
# In seconds.
#
# Arguments:
#   *filePath           [IN]   The full iRODS path of the object
#   *age                [OUT]  The age of the object 
#
# Author: Giacomo Mariani, CINECA
#
EUDATgetObjectTimeDiff(*filePath, *age) {
    # Look when the file has been created in iRODS
    msiSplitPath(*filePath, *fileDir, *fileName);   
    *ec = SELECT DATA_CREATE_TIME, DATA_MODIFY_TIME WHERE DATA_NAME = '*fileName' AND COLL_NAME = '*fileDir';
    foreach(*ec) {
        msiGetValByKey(*ec, "DATA_CREATE_TIME", *creationTime);
        msiWriteRodsLog("EUDATgetObjectTimeDiff -> Created at  *creationTime", *status);
        msiGetValByKey(*ec, "DATA_MODIFY_TIME", *modifyTime);
        msiWriteRodsLog("EUDATgetObjectTimeDiff -> Modified at *modifyTime", *status);
    }
    *age=int(*modifyTime)-int(*creationTime);
    msiWriteRodsLog("EUDATgetObjectTimeDiff -> Difference in time: *age seconds", *status);
}

#
# Rules to chech if a file is in a given path.
#
# Arguments:
#   *path               [IN]    The full iRODS path of the object
#   *subColl            [IN]    The iRODS path of the collection we are looging in for the object
#   *b                  [REI]   False if no value is found, trou elsewhere
#
# Author: Hao Xu, DICE; Giacomo Mariani, CINECA
#
EUDATfileInPath(*path,*subColl) {
    writeLine ("serverLog","conditional acPostProcForCopy -> EUDATfileInPath");
    msiSplitPath(*path, *coll, *name);
    *b = bool("false");
    #*fullColl = str(*subColl)++"/%"
    #writeLine ("serverLog","EUDATfileInPath -> fullColl = *fullColl");
    #*d = SELECT count(DATA_NAME) WHERE COLL_NAME like '*fullColl' AND DATA_NAME = '*name';
    *d = SELECT count(DATA_NAME) WHERE COLL_NAME like '*subColl' AND DATA_NAME = '*name';
    foreach(*c in *d) {
         msiGetValByKey(*c,"DATA_NAME",*num);
         if(*num == '1') {
             writeLine("serverLog","EUDATfileInPath -> found file *name in collection *subColl");
             *b = bool("true");
         }   
    }
    *b;
} 

#
#
# Create a PID for *source and store its value and the object checksum in the iCAT if it does ot exist.
# Otherwhise modify the PID.
#
# Arguments:
#   *source          [IN]    Path of the source file
#   *pid             [OUT]   PID of the source file
#
# Author: Giacomo Mariani, CINECA
# Edited: Elena Erastova, RZG
#
EUDATeiPIDeiChecksumMgmt2(*source, *pid) {
    msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> Look if the PID is in the iCAT", *status);
    # Search for iPID and, if it exists, enter the if below
    if (EUDATiFieldVALUEretrieve(*source, "PID", *pid))
    {
        msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> Update PID with CHECKSUM for: *pid, $userNameClient, *source", *status);
        EUDATeCHECKSUMupdate2(*pid,*source);                           # Update the eCHECKSUM.
    }
    # iPID does not exist
    else
    {
        # If *ePIDcheck look for ePID
        *pid = "empty";
        msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> No PID registered in iCAT. Looking on the EPIC server.", *status);
        getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
        # Get ePID looking for one between: URL and CHECKSUM.
        #msiDataObjChksum(*source, "null", *checksum);
        EUDATePIDsearch("URL", "*serverID"++"*source", *pid);
        # If ePID does not exist create both ePID and iPID
        if ( *pid == "empty" ) then
            {
            msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> No PID in epic server yet", *status);
            EUDATePIDcreate(*source, *pid);                            # Create ePID
            EUDATiPIDcreate(*source, *pid);                            # Create iPID
            }
        # ePID exists. Edit checksum in the ePID and create iPID
        else
        {
            msiWriteRodsLog("EUDATeiPIDeiChecksumMgmt -> Modifying the PID in epic server: *pid", *status);
            EUDATeCHECKSUMupdate2(*pid,*source);                       # Update eCHECKSUM
            EUDATiPIDcreate(*source, *pid);                            # Create iPID
        }
    }
}

#
# Walk through the collection. For each object in the collection 
# it creates a PID and stores its value and the object checksum in the iCAT if it does ot exist.
# Otherwhise the function modify the PID.
#
# Arguments:
#   *sourceColl      [IN]    Source colection path
#
# Author: Elena Erastova, RZG
#
EUDATeiPIDeiChecksumMgmtColl(*sourceColl) {
    *Work=``{
        msiGetObjectPath(*File,*source,*status);
        msiWriteRodsLog("EUDATeiPIDeiChecksumMgmtColl: File *source", *status);
        EUDATeiPIDeiChecksumMgmt2(*source, *pid);
        EUDATiRORupdate2(*source, *pid);
        }``;
        msiCollectionSpider(*sourceColl,*File,*Work,*Status);
}

#
# Add the ROR field of the PID of the object to iCAT
#
# Arguments:
#   *source            [IN] Object path
#   *pid               [IN] Object pid
#
# Author: Elena Erastova, RZG
#
EUDATiRORupdate2(*source, *pid) {
    if (!EUDATiFieldVALUEretrieve(*source, "EUDAT/ROR", *ror)) {
        getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug);
        msiWriteRodsLog("EUDATiRORupdate -> modify ROR in iCAT related to to the path *source", *status);
        msiExecCmd("epicclient.py", "*credStoreType *credStorePath read *pid --key ROR", "null", "null", "null", *out);
        msiGetStdoutInExecCmdOut(*out, *ror);
        msiGetObjType(*source, *objType);
        if(*ror=="None") {
            msiExecCmd("epicclient.py", "*credStoreType *credStorePath read *pid --key EUDAT/ROR", "null", "null", "null", *out);
            msiGetStdoutInExecCmdOut(*out, *ror);
            if(*ror=="None") {
                msiWriteRodsLog("EUDATiRORupdate -> NO ROR for *pid ", *status);
            }
            else {
                *KVString="EUDAT/ROR=*ror";
                msiString2KeyValPair(*KVString,*KVPair);
                msiAssociateKeyValuePairsToObj(*KVPair,*source,*objType);
            }
        }
        else {
            *KVString="EUDAT/ROR=*ror";
            msiString2KeyValPair(*KVString,*KVPair);
            msiAssociateKeyValuePairsToObj(*KVPair,*source,*objType);
        }
        msiWriteRodsLog("EUDATiRORupdate -> saved ROR *ror for PID *pid ", *status);
    }
}

