
#ifndef YUNSPACEBREW_H
#define YUNSPACEBREW_H

#include "Arduino.h"
#include <Bridge.h>
#include <Console.h>
#include <Process.h>

enum SBmsg { 
	CONNECTION_START = char(28), 
	CONNECTION_END = char(27), 
	CONNECTION_ERROR = char(26), 
	MSG_CONFIRM = char(7), 
	MSG_START = char(29), 
	MSG_DIV = char(30), 
	MSG_END = char(31) 
};



int const pidLength = 6;
int const sbPidsLen = 4;

class SpacebrewYun {

	public:

		SpacebrewYun(const String&);

		void connect();

		void monitor();
		void onMessage();
		void onConfirm();

		boolean connected();

		void send(const String&);
		
		typedef void (*OnStringMessage)(String name, String value);
		typedef void (*OnSBOpen)();
		typedef void (*OnSBClose)();
		typedef void (*OnSBError)(int code, String message);

		void onOpen(OnSBOpen function);
		void onClose(OnSBClose function);
		void onStringMessage(OnStringMessage function);
		void onError(OnSBError function);

	private:

		Process brew;
		String name;
		String server;
		String description;
		boolean _started;
		boolean _connected;
		boolean _error_msg;
		int port;

		/**Output should be at least 5 cells**/
        static OnStringMessage _onStringMessage;
		static OnSBOpen _onOpen;
		static OnSBClose _onClose;
		static OnSBError _onError;

		String sub_name;
		String sub_msg;
		String sub_type;

		boolean read_name;
		boolean read_msg;
		boolean read_confirm;
		static int sub_name_max;
		static int sub_msg_str_max;

		long connect_attempt;
		int connect_attempt_inter;

		Process pids;
		char pid [6];
		int sbPids [4];

		void killPids();
		void getPids();

		static char * createString(int len){
			char * out = ( char * ) malloc ( len + 1 );
			return out;
		}		

		static void emptyString(char * str, int len){
			for (int i = 0; i < len; i++) {
				str[i] = '\0';
			}
		}

};

#endif
