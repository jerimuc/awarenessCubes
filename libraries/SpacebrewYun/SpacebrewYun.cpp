#include "SpacebrewYun.h"


SpacebrewYun::SpacebrewYun(const String& _name) {
	name = _name;
	description = "cube";
	//subscribers = NULL;
	//publishers = NULL;

	server = "pure-headland-6826.herokuapp.com";
	port = 80;

	_started = false;
	_connected = false;
	_error_msg = false;

	sub_name = "";
	sub_msg = "";
	sub_type = "";

	read_name = false;
	read_msg = false;

	connect_attempt = 0;
	connect_attempt_inter = 10000;
    

	for ( int i = 0; i < pidLength; i++ ) {
		pid [i] = '\0';
	}

	for ( int i = 0; i < sbPidsLen; i++ ) {
		sbPids [i] = '\0';
	}
    
    

	Console.buffer(64);

}

int SpacebrewYun::sub_msg_str_max = 50;
int SpacebrewYun::sub_name_max = 20;

char pubName[] = "Mout";
char pubType[] = "string";
char *pubLastMsg;
int pubConfirmed = false;
long pubTime = 0;
char subName[] = "Min";
char subType[] = "string";

SpacebrewYun::OnStringMessage SpacebrewYun::_onStringMessage = NULL;
SpacebrewYun::OnSBOpen SpacebrewYun::_onOpen = NULL;
SpacebrewYun::OnSBClose SpacebrewYun::_onClose = NULL;
SpacebrewYun::OnSBError SpacebrewYun::_onError = NULL;

void SpacebrewYun::onOpen(OnSBOpen function){
	_onOpen = function;
}
void SpacebrewYun::onClose(OnSBClose function){
	_onClose = function;
}
void SpacebrewYun::onStringMessage(OnStringMessage function){
	_onStringMessage = function;
}
void SpacebrewYun::onError(OnSBError function){
	_onError = function;
}

void SpacebrewYun::connect() {
    
    pubLastMsg = createString(sub_msg_str_max);
    emptyString(pubLastMsg, sub_msg_str_max);
	_started = true;
	connect_attempt = millis();

	killPids();

	brew.begin("run-spacebrew"); // Process should launch the "curl" command
	// brew.begin("python"); // Process should launch the "curl" command
	// brew.addParameter("/usr/lib/python2.7/spacebrew/spacebrew.py"); // Process should launch the "curl" command
	brew.addParameter("--server");
	brew.addParameter(server);
	brew.addParameter("--port");
	brew.addParameter(String(port));
	brew.addParameter("-n");
	brew.addParameter(name);
	brew.addParameter("-d");
	brew.addParameter(description);
    
    brew.addParameter("-s"); // Add the URL parameter to "curl"
    brew.addParameter(subName); // Add the URL parameter to "curl"
    brew.addParameter(","); // Add the URL parameter to "curl"
    brew.addParameter(subType); // Add the URL parameter to "curl"
    
    brew.addParameter("-p"); // Add the URL parameter to "curl"
    brew.addParameter(pubName); // Add the URL parameter to "curl"
    brew.addParameter(","); // Add the URL parameter to "curl"
    brew.addParameter(pubType); // Add the URL parameter to "curl"

	Console.begin();

	brew.runAsynchronously();
    
	while (!Console) { ; }
}

void SpacebrewYun::monitor() {

	// if not connected try to reconnect after appropriate interval
	if (_started && !_connected) {
		if ((millis() - connect_attempt) > connect_attempt_inter) {
			connect();
		}
	}

	// if message received from console, then process it
	while (Console.available() > 0) {
		char c = Console.read();

		if (c == char(CONNECTION_START) && _started && !_connected) {
            
			if (_onOpen != NULL){
				_onOpen();
			}
			_connected = true;
		} 	    

		else if (c == char(CONNECTION_END) && _connected) {
			_connected = false;
            
			if (_onClose != NULL){
				_onClose();
			}
		} 	 
        

		if (_connected) {
			// set flag to read data message name
			if (c == char(MSG_START)) {
				read_name = true;

			// set flag to read data message payload
			} else if (c == char(MSG_DIV) || sub_name.length() > sub_name_max) {
				read_name = false;
				read_msg = true;

			// set flag to read confirm message
			} else if (c == char(MSG_CONFIRM)) {
				read_confirm = true;

			// process data or confirm message, or reset message
			} else if (c == char(MSG_END) || sub_msg.length() > sub_msg_str_max) {
				if (read_msg == true) {
					onMessage();
				}
				if (read_confirm == true) {
					onConfirm();
					delay(2);
				}

				read_confirm = false;
				read_msg = false;
				sub_name = "";
				sub_msg = "";
				sub_type = "";

				// send a message received confirmation
				Console.print(char(7));

			// read message body
			} else {
				if (read_name == true) {
					sub_name += c;
				} else if (read_confirm == true) {
					sub_name += c;
				} else if (read_msg == true) {
					sub_msg += c;
				} 
				    	
			}
		}
	}	

	// check if received confirmation that linino received messages 
	if (_connected) {
			
        if ( (pubConfirmed == 0) && ((millis() -  pubTime) > 50) ) {
            send(pubLastMsg);
        }
	}
}

void SpacebrewYun::onConfirm() {
			if (sub_name.equals(pubName) == true) {
				pubConfirmed = true;
			}
	
}

boolean SpacebrewYun::connected() {
	return SpacebrewYun::_connected;
}


void SpacebrewYun::onMessage() {

	if (sub_name.equals("") == false) {
		
		while(sub_type.equals("") == true){
			if (sub_name.equals(subName) == true) {
				sub_type = subType;
			}
		}
	}

    if (_onStringMessage != NULL) {
        _onStringMessage( sub_name, sub_msg );
		
	}

}


void SpacebrewYun::send(const String& value){
	
		Console.print(char(29));
		Console.print(pubName);
		Console.print(char(30));
		Console.print(value);
		Console.print(char(31));
		Console.flush();			

        int msg_len = 0;

        msg_len = sub_msg_str_max;

        if (value.length() < msg_len) msg_len = value.length() + 1;
        value.toCharArray(pubLastMsg, msg_len);

        pubConfirmed = false;
        pubTime = millis();

}


/**
 * method that gets the pid of all spacebrew.py instances running on the linino.
 */
void SpacebrewYun::getPids() {

	// request the pid of all python processes
	// brew.begin("run-getsbpids"); // Process should launch the "curl" command
	pids.begin("python");
	pids.addParameter("/usr/lib/python2.7/spacebrew/getprocpid.py"); // Process should launch the "curl" command
	pids.run();

	int sbPidsIndex = 0;
	int pidCharIndex = 0;
	char c = '\0';

	while ( pids.available() > 0 ) {

	    c = pids.read();

		if ( c >= '0' && c <= '9' ) {
			pid[pidCharIndex] = c;
			pidCharIndex = (pidCharIndex + 1) % pidLength;
		} 

		else if ( (c == ' ' || c == '\n') && pidCharIndex > 0) {
			sbPids[sbPidsIndex] = atoi(pid);
			if ( sbPidsIndex < (sbPidsLen - 1) ) sbPidsIndex = (sbPidsIndex + 1);    		

			for( int i = 0; i < pidLength; i++ ){ 
				pid[i] = '\0';
				pidCharIndex = 0;
			}
		}
	}
}

/**
 * method that kills all of the spacebrew.py instances that are running 
 * on the linino.
 */
void SpacebrewYun::killPids() {
	getPids();
	delay(400);

	for (int i = 0; i < sbPidsLen; i ++) {
		if (sbPids[i] > 0) {
			char * newPID = itoa(sbPids[i], pid, 10);

			Process p;
			p.begin("kill");
			p.addParameter("-9");
			p.addParameter(newPID);		// Process should launch the "curl" command
			p.run();            		// Run the process and wait for its termination	

			delay(400);						
		}
	}
}


