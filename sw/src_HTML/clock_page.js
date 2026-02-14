/*
   Eclipse Paho MQTT-JS Utility
   by Elliot Williams for Hackaday article

   Hacked up by:  Mark McDermott for EE445L Lab 4E
   On:            5/29/23   
*/

// ---------------------------------------
// Global variables
//

var client   = null;
var hour     = 0;
var minute   = 0;
var second   = 0;
var mil_time = 0;   // 0 = 12-hour, 1 = 24-hour

// TODO: update eid with your own.
var hostname        = "192.168.86.57";
var port            = "9001";
var eid             = "nm34484_sds4578"
var clientId        = "mqtt_ee445l_" + eid;

// Subscribe (board → webpage). Board publishes time, mode, and display options here.
var test    = eid + "/test";
var hour_bd = eid + "/b2w/hour";
var min_bd  = eid + "/b2w/min";
var sec_bd  = eid + "/b2w/sec";
var mode_bd = eid + "/b2w/mode";
var mil_bd  = eid + "/b2w/mil";   // 12/24: 0 = 12hr, 1 = 24hr (optional)

// Publish (webpage → board): MUST be /w2b so ESP receives it (ESP subscribes to eid/w2b). NOT b2w!
// 1=12/24, 2=inc, 3=dec, 4=mode cycle, 5=theme, 6=speaker freq
var w2b = eid + "/w2b";   // do not change to b2w

// -----------------------------------------------------------------------
// This is called after the webpage is completely loaded
// It is the main entry point into the JS code

function connect() {
	// Set up the client
	// Use same broker as ESP (hostname + port) so w2b messages reach the board
	const url = 'ws://' + hostname + ':' + port + '/mqtt';

	const options = {
		clean: true,
		connectTimeout: 4000,
		// Use unique ID so we don't steal the ESP's session (ESP uses "ee445l-mqtt-" + eid)
		clientId: "web_" + eid + "_" + Date.now(),
		username: null,
		password: null,
	};
	client  = mqtt.connect(url, options);
	client.on('connect', function () {
		onConnect();
	});

	// Receive messages
	client.on('message', function (topic, payload, packet) {
	  	onMessageArrived(topic, payload);
	});
}

function onConnect() {
	console.log("Client Connected.");
    
	client.subscribe(test);
	client.subscribe(hour_bd);
	client.subscribe(min_bd);
	client.subscribe(sec_bd);
	client.subscribe(mode_bd);
	client.subscribe(mil_bd);

	console.log("Subscribed to:", [test, hour_bd, min_bd, sec_bd, mode_bd, mil_bd].join(", "));
}

function payloadToString(message) {
	if (message == null) return "";
	if (typeof message === "string") return message.trim();
	if (message.toString && typeof message.toString === "function") return message.toString().trim();
	if (typeof Buffer !== "undefined" && message instanceof Buffer) return message.toString("utf8").trim();
	if (message.length >= 0 && typeof message.subarray === "function") {
		try { return String.fromCharCode.apply(null, message).trim(); } catch (e) { }
	}
	return String(message).trim();
}

function onMessageArrived(topic, message) {
	var msg = payloadToString(message);
	var t = (typeof topic === 'string') ? topic : (topic != null && topic.toString ? topic.toString() : String(topic));
	console.log(t, msg);

	var timeUpdated = false;
	switch (t) {
		case test:
			console.log("Test message!");
			break;
		case hour_bd:
			var h = parseInt(msg, 10);
			if (!isNaN(h) && ((h >= 0 && h <= 23) || (h >= 1 && h <= 12))) { hour = h; timeUpdated = true; }
			break;
		case min_bd:
			var mn = parseInt(msg, 10);
			if (!isNaN(mn) && mn >= 0 && mn <= 59) { minute = mn; timeUpdated = true; }
			break;
		case sec_bd:
			var s = parseInt(msg, 10);
			if (!isNaN(s) && s >= 0 && s <= 59) { second = s; timeUpdated = true; }
			break;
		case mode_bd:
			var m = parseInt(msg, 10);
			if (!isNaN(m) && m >= 0 && m <= 3) {
				currentMode = m;
				var el = document.getElementById("mode-label");
				if (el) el.textContent = "Mode: " + modeLabels[currentMode];
			}
			break;
		case mil_bd:
			var mil = parseInt(msg, 10);
			if (mil === 0 || mil === 1) { mil_time = mil; timeUpdated = true; }
			break;
		default:
			break;
	}
	if (timeUpdated) updateBoardClockDisplay();
}

// -----------------------------------------------------------------------
// Publish to a specific topic (webpage → board)
//
function publishTo(topic, payload) {
	if (!client || !client.connected) {
		console.warn("MQTT not connected, cannot publish:", topic, payload);
		return;
	}
	var pl = String(payload);
	// Topic must end with /w2b for the ESP to receive (ESP subscribes to eid/w2b)
	console.log("Publish topic=\"" + topic + "\" payload=\"" + pl + "\" (expect .../w2b)");
	client.publish(topic, pl);
}

// -----------------------------------------------------------------------
// All buttons publish to topic w2b with payload "1"–"6"
// 1=12/24, 2=inc, 3=dec, 4=mode cycle, 5=theme, 6=speaker freq
// -----------------------------------------------------------------------
function toggleMode() {
	publishTo(w2b, "1");
}
function toggle_mode() { toggleMode(); }

function increment() {
	publishTo(w2b, "2");
}
function decrement() {
	publishTo(w2b, "3");
}
function dec_hour() { decrement(); }

var currentMode = 0;
var modeLabels = ["Set alarm hour", "Set alarm minute", "Set clock hour", "Set clock minute"];

function cycle_mode() {
	// Don't optimistically update the label - board is source of truth. Publish only; label updates when b2w/mode arrives.
	publishTo(w2b, "4");
}

var isDark = false;

function toggle_theme() {
	isDark = !isDark;
	document.body.classList.toggle("dark", isDark);
	publishTo(w2b, "5");
}

function cycle_freq() {
	publishTo(w2b, "6");
}

function publish(payload) {
	publishTo(w2b, payload);
}


// -----------------------------------------------------------------------
// Write current hour/minute/second to the board-clock element (single source of truth)
//
function updateBoardClockDisplay() {
	var el = document.getElementById("board-clock");
	if (!el) return;

	var displayHour = hour;
	var period = "";

	if (mil_time === 0) {
		if (hour === 0) {
			displayHour = 12;
			period = "AM";
		} else if (hour <= 11) {
			displayHour = hour;
			period = "AM";
		} else if (hour === 12) {
			displayHour = 12;
			period = "PM";
		} else {
			displayHour = hour - 12;
			period = "PM";
		}
	}

	var h = update(displayHour);
	var m = update(minute);
	var s = update(second);
	el.innerText = h + " : " + m + " : " + s + (period ? " " + period : "");
}

// -----------------------------------------------------------------------
// Board time display: refresh every 1s and on MQTT updates
//
function Board_Time() {
	updateBoardClockDisplay();
	setTimeout(Board_Time, 1000);
}

function update(t) {
	if (t < 10) {
		return "0" + t;
	}
	else {
		return t;
	}
}

Board_Time();
