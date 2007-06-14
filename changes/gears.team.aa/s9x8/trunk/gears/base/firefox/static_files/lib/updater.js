var global=this;function isDef(val){return typeof val!="undefined"}String.prototype.startsWith=function(prefix){return this.indexOf(prefix)==0};String.prototype.endsWith=function(suffix){var l=this.length-suffix.length;return l>=0&&this.lastIndexOf(suffix,l)==l};String.prototype.trim=function(){return this.replace(/^\s+|\s+$/g,"")};String.prototype.subs=function(){var ret=this;for(var i=0;i<arguments.length;i++){ret=ret.replace(/\%s/,String(arguments[i]))}return ret};if(!Function.prototype.apply){Function.prototype.apply=
function(oScope,opt_args){var sarg=[],rtrn,call;if(!oScope)oScope=global;var args=opt_args||[];for(var i=0;i<args.length;i++){sarg[i]="args["+i+"]"}call="oScope.__applyTemp__.peek()("+sarg.join(",")+");";if(!oScope.__applyTemp__){oScope.__applyTemp__=[]}oScope.__applyTemp__.push(this);rtrn=eval(call);oScope.__applyTemp__.pop();return rtrn}}if(!Array.prototype.push){Array.prototype.push=function(){for(var i=0;i<arguments.length;i++){this[this.length]=arguments[i]}return this.length}}if(!Array.prototype.pop){Array.prototype.pop=
function(){if(!this.length){return}var val=this[this.length-1];this.length--;return val}}Array.prototype.peek=function(){return this[this.length-1]};if(!Array.prototype.shift){Array.prototype.shift=function(){if(this.length==0){return}var val=this[0];for(var i=0;i<this.length-1;i++){this[i]=this[i+1]}this.length--;return val}}if(!Array.prototype.unshift){Array.prototype.unshift=function(){var numArgs=arguments.length;for(var i=this.length-1;i>=0;i--){this[i+numArgs]=this[i]}for(var j=0;j<numArgs;j++){this[j]=
arguments[j]}return this.length}}if(!Array.prototype.forEach){Array.prototype.forEach=function(callback,opt_scope){for(var i=0;i<this.length;i++){callback.call(opt_scope,this[i],i,this)}}}function bind(fn,self,var_args){var boundargs=fn.boundArgs_||[];boundargs=boundargs.concat(Array.prototype.slice.call(arguments,2));if(typeof fn.boundSelf_!="undefined"){self=fn.boundSelf_}if(typeof fn.boundFn_!="undefined"){fn=fn.boundFn_}var newfn=function(){var args=boundargs.concat(Array.prototype.slice.call(arguments));
return fn.apply(self,args)};newfn.boundArgs_=boundargs;newfn.boundSelf_=self;newfn.boundFn_=fn;return newfn}Function.prototype.bind=function(self){return bind.apply(null,[this,self].concat(Array.prototype.slice.call(arguments,1)))};Function.prototype.partial=function(){return bind.apply(null,[this,null].concat(Array.prototype.slice.call(arguments)))};Function.prototype.inherits=function(parentCtor){var tempCtor=function(){};tempCtor.prototype=parentCtor.prototype;this.superClass_=parentCtor.prototype;
this.prototype=new tempCtor};Function.prototype.mixin=function(props){for(var x in props){this.prototype[x]=props[x]}if(typeof props["toString"]=="function"&&props["toString"]!=this.prototype["toString"]){this.prototype.toString=props.toString}};function G_Preferences(opt_startPoint,opt_getDefaultBranch,opt_noUnicode){this.debugZone="prefs";this.observers_={};var startPoint=opt_startPoint||null,prefSvc=Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService);if(opt_getDefaultBranch){this.prefs_=prefSvc.getDefaultBranch(startPoint)}else{this.prefs_=prefSvc.getBranch(startPoint)}this.prefs_.QueryInterface(Ci.nsIPrefBranchInternal);this.noUnicode_=!(!opt_noUnicode)}G_Preferences.setterMap_={string:"setCharPref","boolean":"setBoolPref",
number:"setIntPref"};G_Preferences.getterMap_={};G_Preferences.getterMap_[Ci.nsIPrefBranch.PREF_STRING]="getCharPref";G_Preferences.getterMap_[Ci.nsIPrefBranch.PREF_BOOL]="getBoolPref";G_Preferences.getterMap_[Ci.nsIPrefBranch.PREF_INT]="getIntPref";G_Preferences.prototype.setPref=function(key,val){var datatype=typeof val;if(datatype=="number"&&val%1!=0){throw new Error("Cannot store non-integer numbers in preferences.");}if(datatype=="string"&&!this.noUnicode_){return this.setUnicodePref(key,val)}var meth=
G_Preferences.setterMap_[datatype];if(!meth){throw new Error("Pref datatype {"+datatype+"} not supported.");}return this.prefs_[meth](key,val)};G_Preferences.prototype.getPref=function(key,opt_default){var type=this.prefs_.getPrefType(key);if(type==Ci.nsIPrefBranch.PREF_INVALID){return opt_default}if(type==Ci.nsIPrefBranch.PREF_STRING&&!this.noUnicode_){return this.getUnicodePref(key,opt_default)}var meth=G_Preferences.getterMap_[type];if(!meth){throw new Error("Pref datatype {"+type+"} not supported.");
}try{return this.prefs_[meth](key)}catch(e){return opt_default}};G_Preferences.prototype.setUnicodePref=function(key,value){var s=Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);s.data=value;return this.prefs_.setComplexValue(key,Ci.nsISupportsString,s)};G_Preferences.prototype.getUnicodePref=function(key,opt_default){try{return this.prefs_.getComplexValue(key,Ci.nsISupportsString).data}catch(e){return opt_default}};G_Preferences.prototype.setBoolPref=function(which,value){return this.setPref(which,
value)};G_Preferences.prototype.getBoolPref=function(which){return this.prefs_.getBoolPref(which)};G_Preferences.prototype.getBoolPrefOrDefault=function(which,def){return this.getPref(which,def)};G_Preferences.prototype.getBoolPrefOrDefaultAndSet=function(which,def){try{return this.prefs_.getBoolPref(which)}catch(e){this.prefs_.setBoolPref(which,!(!def));return def}};G_Preferences.prototype.clearPref=function(which){try{this.prefs_.clearUserPref(which)}catch(e){}};G_Preferences.prototype.addObserver=
function(which,callback){var observer=new G_PreferenceObserver(callback);if(!this.observers_[which])this.observers_[which]=new G_ObjectSafeMap;this.observers_[which].insert(callback,observer);this.prefs_.addObserver(which,observer,false)};G_Preferences.prototype.removeObserver=function(which,callback){var observer=this.observers_[which].find(callback);this.prefs_.removeObserver(which,observer);this.observers_[which].erase(callback)};G_Preferences.prototype.getChildNames=function(opt_startingPoint){if(!opt_startingPoint){opt_startingPoint=
""}return this.prefs_.getChildList(opt_startingPoint,{})};G_Preferences.savePrefFile=function(){var prefService=Cc["@mozilla.org/preferences;1"].getService(Ci.nsIPrefService);try{prefService.savePrefFile(null)}catch(e){}};function G_PreferenceObserver(callback){this.debugZone="prefobserver";this.callback_=callback}G_PreferenceObserver.prototype.observe=function(subject,topic,data){this.callback_(data)};G_PreferenceObserver.prototype.QueryInterface=function(iid){var Ci=Ci;if(iid.equals(Ci.nsISupports)||
iid.equals(Ci.nsIObserver)||iid.equals(Ci.nsISupportsWeakReference))return this;throw Components.results.NS_ERROR_NO_INTERFACE;};if(!isDef(false)){throw new Error("G_GDEBUG constant must be set before loading debug.js");}function G_Debug(who,msg){}function G_DebugL(who,msg){}function G_DebugZone(service,prefix,zone){}G_DebugZone.prototype.zoneIsEnabled=function(){};G_DebugZone.prototype.enableZone=function(){};G_DebugZone.prototype.disableZone=function(){};G_DebugZone.prototype.debug=function(msg){};G_DebugZone.prototype.error=function(msg){};G_DebugZone.prototype.assert=function(condition,msg){};function G_DebugService(opt_prefix){}
G_DebugService.ERROR_LEVEL_INFO="INFO";G_DebugService.ERROR_LEVEL_WARNING="WARNING";G_DebugService.ERROR_LEVEL_EXCEPTION="EXCEPTION";G_DebugService.TIMESTAMP_INTERVAL=300000;G_DebugService.prototype.QueryInterface=function(iid){if(iid.equals(Ci.nsISupports)||iid.equals(Ci.nsITimerCallback)){return this}throw Components.results.NS_ERROR_NO_INTERFACE;};G_DebugService.prototype.alsoDumpToShell=function(){};G_DebugService.prototype.alsoDumpToConsole=function(){};G_DebugService.prototype.logFileIsEnabled=
function(){};G_DebugService.prototype.enableLogFile=function(){};G_DebugService.prototype.disableLogFile=function(){};G_DebugService.prototype.getLogFile=function(){};G_DebugService.prototype.setLogFile=function(file){};G_DebugService.prototype.closeLogFile=function(){this.logWriter_.close();this.logWriter_=null};G_DebugService.prototype.enableDumpToShell=function(){};G_DebugService.prototype.disableDumpToShell=function(){};G_DebugService.prototype.enableDumpToConsole=function(){};G_DebugService.prototype.disableDumpToConsole=
function(){};G_DebugService.prototype.getZone=function(zone){};G_DebugService.prototype.enableZone=function(zone){};G_DebugService.prototype.disableZone=function(zone){};G_DebugService.prototype.allZonesEnabled=function(){};G_DebugService.prototype.enableAllZones=function(){};G_DebugService.prototype.disableAllZones=function(){};G_DebugService.prototype.callTracingEnabled=function(){};G_DebugService.prototype.enableCallTracing=function(){};G_DebugService.prototype.disableCallTracing=function(){};
G_DebugService.prototype.getLogFileErrorLevel=function(){};G_DebugService.prototype.setLogFileErrorLevel=function(level){};G_DebugService.prototype.notify=function(timer){if(!this.activeSinceLastTimestamp_){return}this.dump(G_File.LINE_END_CHAR+"=========== Date: "+(new Date).toLocaleString()+" ============"+G_File.LINE_END_CHAR+G_File.LINE_END_CHAR);this.activeSinceLastTimestamp_=false};G_DebugService.prototype.dump=function(msg){};G_DebugService.prototype.maybeDumpToFile=function(msg){if(this.logFileIsEnabled()&&
this.logFile_){if(!this.logWriter_){this.logWriter_=new G_FileWriter(this.logFile_,true)}this.logWriter_.write(msg)}};G_DebugService.prototype.observe=function(consoleMessage){};G_DebugService.prototype.reportScriptError_=function(message,sourceName,lineNumber,label){if(sourceName.startsWith("http")){return}var message=["","------------------------------------------------------------",label+": "+message,"location: "+sourceName+", line: "+lineNumber,"------------------------------------------------------------",
"",""].join(G_File.LINE_END_CHAR);if(this.alsoDumpToShell()){dump(message)}this.maybeDumpToFile(message)};function G_Loggifier(){}G_Loggifier.prototype.mark_=function(obj){};G_Loggifier.prototype.isLoggified=function(obj){};G_Loggifier.prototype.getFunctionName_=function(constructor){};G_Loggifier.prototype.loggify=function(obj){};function G_DebugSettings(){this.defaults_={};this.prefs_=new G_Preferences}G_DebugSettings.prototype.getSetting=function(name,opt_default){var override=this.prefs_.getPref(name,
null);if(override!==null){return override}else if(name in this.defaults_){return this.defaults_[name]}else{return opt_default}};G_DebugSettings.prototype.setDefault=function(name,val){this.defaults_[name]=val};var G_debugService=new G_DebugService;var G_File={};G_File.getHomeFile=function(opt_file){return this.getSpecialFile("Home",opt_file)};G_File.getProfileFile=function(opt_file){return this.getSpecialFile("ProfD",opt_file)};G_File.getTempFile=function(opt_file){return this.getSpecialFile("TmpD",opt_file)};G_File.getSpecialFile=function(loc,opt_file){var file=Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties).get(loc,Ci.nsILocalFile);if(opt_file){file.append(opt_file)}return file};G_File.createUniqueTempFile=function(opt_baseName){var baseName=
(opt_baseName||(new Date).getTime())+".tmp",file=this.getSpecialFile("TmpD",baseName);file.createUnique(file.NORMAL_FILE_TYPE,420);return file};G_File.createUniqueTempDir=function(opt_baseName){var baseName=(opt_baseName||(new Date).getTime())+".tmp",dir=this.getSpecialFile("TmpD",baseName);dir.createUnique(dir.DIRECTORY_TYPE,484);return dir};G_File.fromFileURI=function(uri){if(uri.indexOf("file://")!=0)throw new Error("File path must be a file:// URL");var fileHandler=Cc["@mozilla.org/network/protocol;1?name=file"].getService(Ci.nsIFileProtocolHandler);
return fileHandler.getFileFromURLSpec(uri)};G_File.PR_RDONLY=1;G_File.PR_WRONLY=2;G_File.PR_RDWR=4;G_File.PR_CREATE_FILE=8;G_File.PR_APPEND=16;G_File.PR_TRUNCATE=32;G_File.PR_SYNC=64;G_File.PR_EXCL=128;G_File.__defineGetter__("LINE_END_CHAR",function(){var end_char;if("@mozilla.org/xre/app-info;1"in Cc){end_char=Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS=="WINNT"?"\r\n":"\n"}else{end_char=Cc["@mozilla.org/network/protocol;1?name=http"].getService(Ci.nsIHttpProtocolHandler).platform.toLowerCase().indexOf("win")==
0?"\r\n":"\n"}G_File.__defineGetter__("LINE_END_CHAR",function(){return end_char});return end_char});function G_FileReader(file){this.file_=typeof file=="string"?G_File.fromFileURI(file):file}G_FileReader.readAll=function(file){var reader=new G_FileReader(file);try{return reader.read()}finally{reader.close()}};G_FileReader.prototype.read=function(opt_maxBytes){if(!this.stream_){var fs=Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);fs.init(this.file_,G_File.PR_RDONLY,
292,0);this.stream_=Cc["@mozilla.org/scriptableinputstream;1"].createInstance(Ci.nsIScriptableInputStream);this.stream_.init(fs)}if(!isDef(opt_maxBytes)){opt_maxBytes=this.stream_.available()}return this.stream_.read(opt_maxBytes)};G_FileReader.prototype.close=function(opt_maxBytes){if(this.stream_){this.stream_.close();this.stream_=null}};function G_FileWriter(file,opt_append){this.file_=typeof file=="string"?G_File.fromFileURI(file):file;this.append_=!(!opt_append)}G_FileWriter.writeAll=function(file,
data,opt_append){var writer=new G_FileWriter(file,opt_append);try{return writer.write(data)}finally{writer.close();return 0}};G_FileWriter.prototype.write=function(data){if(!this.stream_){this.stream_=Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);var flags=G_File.PR_WRONLY|G_File.PR_CREATE_FILE|(this.append_?G_File.PR_APPEND:G_File.PR_TRUNCATE);this.stream_.init(this.file_,flags,-1,0)}return this.stream_.write(data,data.length)};G_FileWriter.prototype.writeLine=
function(data){this.write(data+G_File.LINE_END_CHAR)};G_FileWriter.prototype.close=function(){if(this.stream_){this.stream_.close();this.stream_=null}};function G_ObjectSafeMap(opt_name){this.debugZone="objectsafemap";this.name_=opt_name?opt_name:"noname";this.keys_=[];this.values_=[]}G_ObjectSafeMap.prototype.indexOfKey_=function(key){for(var i=0;i<this.keys_.length;i++)if(this.keys_[i]===key)return i;return-1};G_ObjectSafeMap.prototype.insert=function(key,value){if(key===null)throw new Error("Can't use null as a key");if(value===undefined)throw new Error("Can't store undefined values in this map");var i=this.indexOfKey_(key);if(i==-1){this.keys_.push(key);
this.values_.push(value)}else{this.keys_[i]=key;this.values_[i]=value}};G_ObjectSafeMap.prototype.erase=function(key){var keyLocation=this.indexOfKey_(key),keyFound=keyLocation!=-1;if(keyFound){this.keys_.splice(keyLocation,1);this.values_.splice(keyLocation,1)}return keyFound};G_ObjectSafeMap.prototype.find=function(key){var keyLocation=this.indexOfKey_(key);return keyLocation==-1?undefined:this.values_[keyLocation]};G_ObjectSafeMap.prototype.replace=function(other){this.keys_=[];this.values_=[];
for(var i=0;i<other.keys_.length;i++){this.keys_.push(other.keys_[i]);this.values_.push(other.values_[i])}};G_ObjectSafeMap.prototype.forEach=function(func){if(typeof func!="function")throw new Error("argument to forEach is not a function, it's a(n) "+typeof func);for(var i=0;i<this.keys_.length;i++)func(this.keys_[i],this.values_[i])};G_ObjectSafeMap.prototype.size=function(){return this.keys_.length};function G_Base64(){this.byteToCharMap_={};this.charToByteMap_={};this.byteToCharMapWebSafe_={};this.charToByteMapWebSafe_={};this.init_()}G_Base64.ENCODED_VALS="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";G_Base64.ENCODED_VALS_WEBSAFE="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";G_Base64.prototype.init_=function(){for(var i=0;i<G_Base64.ENCODED_VALS.length;i++){this.byteToCharMap_[i]=G_Base64.ENCODED_VALS.charAt(i);this.charToByteMap_[this.byteToCharMap_[i]]=
i;this.byteToCharMapWebSafe_[i]=G_Base64.ENCODED_VALS_WEBSAFE.charAt(i);this.charToByteMapWebSafe_[this.byteToCharMapWebSafe_[i]]=i}};G_Base64.prototype.encodeByteArray=function(input,opt_webSafe){if(!(input instanceof Array))throw new Error("encodeByteArray takes an array as a parameter");var byteToCharMap=opt_webSafe?this.byteToCharMapWebSafe_:this.byteToCharMap_,output=[],i=0;while(i<input.length){var byte1=input[i],haveByte2=i+1<input.length,byte2=haveByte2?input[i+1]:0,haveByte3=i+2<input.length,
byte3=haveByte3?input[i+2]:0,outByte1=byte1>>2,outByte2=(byte1&3)<<4|byte2>>4,outByte3=(byte2&15)<<2|byte3>>6,outByte4=byte3&63;if(!haveByte3){outByte4=64;if(!haveByte2)outByte3=64}output.push(byteToCharMap[outByte1]);output.push(byteToCharMap[outByte2]);output.push(byteToCharMap[outByte3]);output.push(byteToCharMap[outByte4]);i+=3}return output.join("")};G_Base64.prototype.decodeString=function(input,opt_webSafe){if(input.length%4)throw new Error("Length of b64-encoded data must be zero mod four");
var charToByteMap=opt_webSafe?this.charToByteMapWebSafe_:this.charToByteMap_,output=[],i=0;while(i<input.length){var byte1=charToByteMap[input.charAt(i)],byte2=charToByteMap[input.charAt(i+1)],byte3=charToByteMap[input.charAt(i+2)],byte4=charToByteMap[input.charAt(i+3)];if(byte1===undefined||byte2===undefined||byte3===undefined||byte4===undefined)throw new Error("String contains characters not in our alphabet: "+input);var outByte1=byte1<<2|byte2>>4;output.push(outByte1);if(byte3!=64){var outByte2=
byte2<<4&240|byte3>>2;output.push(outByte2);if(byte4!=64){var outByte3=byte3<<6&192|byte4;output.push(outByte3)}}i+=4}return output};G_Base64.prototype.arrayifyString=function(str){var output=[];for(var i=0;i<str.length;i++){var c=str.charCodeAt(i);while(c>255){output.push(c&255);c>>=8}output.push(c)}return output};G_Base64.prototype.stringifyArray=function(array){var output=[];for(var i=0;i<array.length;i++)output[i]=String.fromCharCode(array[i]);return output.join("")};function G_Alarm(callback,delayMS,opt_repeating,opt_maxTimes){this.debugZone="alarm";this.callback_=callback;this.repeating_=!(!opt_repeating);var Cc=Components.classes,Ci=Components.interfaces;this.timer_=Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);var type=opt_repeating?this.timer_.TYPE_REPEATING_SLACK:this.timer_.TYPE_ONE_SHOT;this.maxTimes_=opt_maxTimes?opt_maxTimes:null;this.nTimes_=0;this.timer_.initWithCallback(this,delayMS,type)}G_Alarm.prototype.cancel=function(){if(this.timer_){this.timer_.cancel();
this.timer_=null;this.callback_=null}};G_Alarm.prototype.notify=function(timer){var ret=this.callback_();this.nTimes_++;if(this.repeating_&&typeof this.maxTimes_=="number"&&this.nTimes_>=this.maxTimes_){this.cancel()}else if(!this.repeating_){this.cancel()}return ret};G_Alarm.prototype.QueryInterface=function(iid){if(iid.equals(Components.interfaces.nsISupports)||iid.equals(Components.interfaces.nsIObserver)||iid.equals(Components.interfaces.nsITimerCallback))return this;throw Components.results.NS_ERROR_NO_INTERFACE;
};function G_ConditionalAlarm(callback,delayMS,opt_repeating,opt_maxTimes){G_Alarm.call(this,callback,delayMS,opt_repeating,opt_maxTimes);this.debugZone="conditionalalarm"}G_ConditionalAlarm.inherits(G_Alarm);G_ConditionalAlarm.prototype.notify=function(timer){var rv=G_Alarm.prototype.notify.call(this,timer);if(this.repeating_&&rv){this.cancel()}};function G_Requester(opt_timeoutMillis){this.status="";this.responseText="";this.timeout_=opt_timeoutMillis?opt_timeoutMillis:10000;this.timeoutExpired_=false}G_Requester.prototype.Open=function(url,options){G_Debug(this,"Sending Async Req To: %s".subs(url));var channel=Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService).newChannel(url,null,null);channel.QueryInterface(Ci.nsIHttpChannel);if(options.postData){var newData=[];for(var i=0;i<options.postData.length;i++){if(options.postData.charCodeAt(i)>
128){G_Debug(this,"Fixing %s".subs(options.postData.charCodeAt(i)));newData.push("&#"+options.postData.charCodeAt(i)+";")}else{newData.push(options.postData.charAt(i))}}newData=newData.join("");var uploadCh=channel.QueryInterface(Ci.nsIUploadChannel),sis=Cc["@mozilla.org/io/string-input-stream;1"].createInstance(Ci.nsIStringInputStream);sis.setData(newData,-1);uploadCh.setUploadStream(sis,"application/xml",-1);channel.requestMethod="POST"}if(isDef(options.username)||isDef(options.password)){var encoder=
new G_Base64,as=encoder.arrayifyString(options.username+":"+options.password),enc=encoder.encodeByteArray(as);channel.setRequestHeader("Authorization","BASIC "+enc,false)}channel.setRequestHeader("Content-Type","application/xml",false);channel.setRequestHeader("Accept-Encoding","compress, gzip",false);channel.setRequestHeader("Connection","close",false);if(options.headers){for(var headerName in options.headers){channel.setRequestHeader(headerName,options.headers[headerName],false)}}channel.asyncOpen(this,
null);this.timer_=new G_Alarm(this.Timeout.bind(this),this.timeout_)};G_Requester.prototype.Timeout=function(){this.timeoutExpired_=true;this.OnFailure()};G_Requester.prototype.onStartRequest=function(request,context){if(this.timeoutExpired_){return false}this.status=request.QueryInterface(Ci.nsIHttpChannel).responseStatus};G_Requester.prototype.onDataAvailable=function(request,context,inputStream,offset,count){if(this.timeoutExpired_){return false}var sis=Cc["@mozilla.org/scriptableinputstream;1"].createInstance(Ci.nsIScriptableInputStream);
sis.init(inputStream);var text=sis.read(count);this.responseText+=text;G_Debug(this,"Streaming: %s".subs(text))};G_Requester.prototype.onStopRequest=function(request,context,status){if(this.timeoutExpired_){return false}this.timer_.cancel();this.OnSuccess()};G_Requester.prototype.OnSuccess=function(){};G_Requester.prototype.OnFailure=function(){};G_Requester.prototype.debugZone="G_Requester";function G_ExtensionUpdater(id,opt_requestOptions){if(!id){return false}this.RDFSvc_=Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);if(opt_requestOptions){this.requestOptions_=opt_requestOptions}else{this.requestOptions_={}}this.id=id;this.updating=false;this.preferences=new G_Preferences("extensions.google.updater.")}G_ExtensionUpdater.prototype.Update=function(){if(this.updating){return false}this.updating=true;this.appVersion="";this.appID="";this.currentVersion="";this.updateURL=
"";this.updateLink="";this.updateVersion="";if(!this.GetCurrent()){this.Failure("'%s' not found in extension manager.".subs(this.name));return false}this.AttemptUpdate()};G_ExtensionUpdater.prototype.UpdatePeriodically=function(opt_makeTimer){var lastUpdate=this.preferences.getPref(this.id,0),timeBetweenUpdates=86400000,nextUpdate=Number(lastUpdate)+timeBetweenUpdates,now=(new Date).getTime();if(now>nextUpdate){this.preferences.setPref(this.id,String(now));this.Update();nextUpdate=now+timeBetweenUpdates}if(opt_makeTimer){this.loop_=
new G_Alarm(this.UpdatePeriodically.bind(this,true),nextUpdate-now,false)}};G_ExtensionUpdater.prototype.CompareVersions=function(aV1,aV2){var v1=aV1.split("."),v2=aV2.split("."),numSubversions=v1.length>v2.length?v1.length:v2.length;for(var i=0;i<numSubversions;i++){if(typeof v2[i]=="undefined"){return 1}if(typeof v1[i]=="undefined"){return-1}if(parseInt(v2[i])>parseInt(v1[i])){return-1}else if(parseInt(v2[i])<parseInt(v1[i])){return 1}}return 0};G_ExtensionUpdater.prototype.GetCurrent=function(){var updItem=
Cc["@mozilla.org/extensions/manager;1"].getService(Ci.nsIExtensionManager).getItemForID(this.id);if(updItem){var appInfo=Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);this.name=updItem.name;this.currentVersion=updItem.version;this.updateURL=updItem.updateRDF;this.updateURL=this.updateURL.replace(/%ITEM_VERSION%/gi,this.currentVersion).replace(/%ITEM_ID%/gi,this.id).replace(/%APP_VERSION%/gi,appInfo.version).replace(/%APP_ID%/gi,appInfo.ID);return true}return false};G_ExtensionUpdater.prototype.AttemptUpdate=
function(){if(!this.updateURL){return false}this.req_=new G_Requester;this.req_.OnSuccess=this.OnReqSuccess.bind(this);this.req_.OnFailure=this.OnReqFailure.bind(this);this.req_.Open(this.updateURL,this.requestOptions_)};G_ExtensionUpdater.prototype.OnReqFailure=function(){this.Failure("OnReqFailure")};G_ExtensionUpdater.prototype.OnReqSuccess=function(){if(this.req_.status!=200||!this.req_.responseText.match(/<rdf/gi)){this.Failure("Error: Invalid Update RDF contents. HTTP Status '%s'".subs(this.req_.status));
return false}var uri=Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService).newURI(this.updateURL,null,null),parser=Cc["@mozilla.org/rdf/xml-parser;1"].createInstance(Ci.nsIRDFXMLParser),memoryDS=Cc["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"].createInstance(Ci.nsIRDFDataSource);parser.parseString(memoryDS,uri,this.req_.responseText);var moz="http://www.mozilla.org/2004/em-rdf#",versionArc=this.RDFSvc_.GetResource(moz+"version"),updateLinkArc=this.RDFSvc_.GetResource(moz+
"updateLink"),thisResource=null,dsResources=memoryDS.GetAllResources();while(dsResources.hasMoreElements()){thisResource=dsResources.getNext().QueryInterface(Ci.nsIRDFResource);var versionRes=memoryDS.GetTarget(thisResource,versionArc,true);if(versionRes){this.updateVersion=versionRes.QueryInterface(Ci.nsIRDFLiteral).Value}var updateLinkRes=memoryDS.GetTarget(thisResource,updateLinkArc,true);if(updateLinkRes){this.updateLink=updateLinkRes.QueryInterface(Ci.nsIRDFLiteral).Value}}if(this.updateVersion&&
this.updateLink){G_Debug(this,"currentVersion:%s\nupdateVersion: %s\nupdateLink: %s".subs(this.currentVersion,this.updateVersion,this.updateLink));if(this.CompareVersions(this.updateVersion,this.currentVersion)==1){this.InstallUpdate()}else{this.Failure("No need to update")}}else{this.Failure("No update info in rdf")}};G_ExtensionUpdater.prototype.InstallUpdate=function(){if(!this.updateLink){this.Failure("Failure");return false}var manager=Cc["@mozilla.org/xpinstall/install-manager;1"].createInstance(Ci.nsIXPInstallManager);
if(manager!=null){G_Debug(this,"UpdateLink: %s".subs(this.updateLink));this.OnUpdateAvailable();var items=[this.updateLink],p=new G_Preferences,autoupdate=p.getPref("extensions.google.%s.autoupdate".subs(this.id.split("@")[0]),true);if(autoupdate==false){G_DebugL(this,"Extension '%s' would've been updated".subs(this.name));this.Success()}else{G_DebugL(this,"Extension '%s' updating...".subs(this.name));manager.initManagerFromChrome(items,items.length,this)}}else{this.Failure("Error creating manager")}};
G_ExtensionUpdater.prototype.onStateChange=function(index,state,value){if(state==Ci.nsIXPIProgressDialog.INSTALL_DONE){if(value!=0){this.Failure("Download Error")}else{this.Success()}}};G_ExtensionUpdater.prototype.onProgress=function(index,value,maxValue){};G_ExtensionUpdater.prototype.Failure=function(aMessage){G_Debug(this,"Failure: %s".subs(aMessage));this.updating=false;this.OnFail(aMessage)};G_ExtensionUpdater.prototype.Success=function(){this.updating=false;this.OnSuccess()};G_ExtensionUpdater.prototype.OnFail=
function(aMessage){};G_ExtensionUpdater.prototype.OnSuccess=function(){};G_ExtensionUpdater.prototype.OnUpdateAvailable=function(){};G_ExtensionUpdater.prototype.debugZone="G_ExtensionUpdater";function G_JSModule(){this.factoryLookup_={};this.categoriesLookup_={}}G_JSModule.prototype.registerObject=function(classID,contractID,className,instance,opt_categories){this.factoryLookup_[classID]=new G_JSFactory(Components.ID(classID),contractID,className,instance,opt_categories);this.categoriesLookup_[classID]=opt_categories};G_JSModule.prototype.registerSelf=function(compMgr,fileSpec,location,type){compMgr=compMgr.QueryInterface(Ci.nsIComponentRegistrar);for(var factory in this.factoryLookup_){compMgr.registerFactoryLocation(this.factoryLookup_[factory].classID,
this.factoryLookup_[factory].className,this.factoryLookup_[factory].contractID,fileSpec,location,type);this.factoryLookup_[factory].registerCategories()}};G_JSModule.prototype.getClassObject=function(compMgr,classID,interfaceID){var factory=this.factoryLookup_[classID.toString()];if(!factory){throw new Error("Invalid classID {%s}".subs(classID));}return factory};G_JSModule.prototype.canUnload=function(){return true};function G_JSFactory(classID,contractID,className,instance,opt_categories){this.classID=
classID;this.contractID=contractID;this.className=className;this.instance_=instance;this.categories_=opt_categories}G_JSFactory.prototype.registerCategories=function(){if(this.categories_){var catMgr=Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);for(var i=0,cat;cat=this.categories_[i];i++){catMgr.addCategoryEntry(cat,this.className,this.contractID,true,true)}}};G_JSFactory.prototype.createInstance=function(outer,iid){return this.instance_};;
