/*jshint sub:true*/
(function () {
'use strict';

// Syntactic sugar
function $(selector) {
	return document.querySelector(selector);
}

// Syntactic sugar & execute callback
function $$(selector, callback) {
	var elems = document.querySelectorAll(selector);
	for (var i = 0; i < elems.length; ++i) {
		if (callback && typeof callback == 'function')
			callback.call(this, elems[i]);
	}
}

var debounce = function (func, wait, now) {
	var timeout;
	return function debounced () {
		var that = this, args = arguments;
		function delayed() {
			if (!now)
				func.apply(that, args);
			timeout = null;
		}
		if (timeout) {
			clearTimeout(timeout);
		} else if (now) {
			func.apply(obj, args);
		}
		timeout = setTimeout(delayed, wait || 250);
	};
};

// global namespace
window.GoAccess = window.GoAccess || {
	initialize: function (options) {
		this.opts = options;
		var cw = Math.max(document.documentElement.clientWidth || 0, window.innerWidth || 0);

		this.AppState = {};
		this.AppTpls = {};
		this.AppCharts = {};
		this.AppUIData = (this.opts || {}).uiData || {};
		this.AppData = (this.opts || {}).panelData || {};
		this.AppWSConn = (this.opts || {}).wsConnection || {};
		this.i18n = (this.opts || {}).i18n || {};
		this.AppPrefs = {
			'autoHideTables': true,
			'layout': cw > 2560 ? 'wide' : 'horizontal',
			'panelOrder': [],
			'perPage': 7,
			'theme': (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ? 'darkPurple' : 'bright',
			'hiddenPanels': [],
		};
		this.AppPrefs = GoAccess.Util.merge(this.AppPrefs, this.opts.prefs);
		this.currentJWT = null;
		this.csrfToken = null;

		// WebSocket reconnection settings
		this.wsDelay = this.currDelay = 1E3;
		this.maxDelay = 20E3;
		this.retries = 0;
		this.maxRetries = 20;
		this.tokenRefreshLeadTime = 60;

		this.handleLocalStorage();
		this.isAppInitialized = false;

		// Initialize message rotation
		this.startMessageRotation();

		// Handle WebSocket setup
		this.handleWebSocketSetup();
	},

	handleLocalStorage: function () {
		// Check if the browser supports localStorage
		if (GoAccess.Util.hasLocalStorage()) {
			// Retrieve the AppPrefs object stored in localStorage
			const ls = JSON.parse(localStorage.getItem('AppPrefs'));
			// Merge stored preferences into the current application preferences
			this.AppPrefs = GoAccess.Util.merge(this.AppPrefs, ls);
		}
	},

	startMessageRotation: function () {
		// Define the messages that will be displayed during the loading process
		const messages = [
			'Fetching authentication token... Please wait.',
			'Validating WebSocket tokens... Please wait.',
			'Authenticating WebSocket connection... Please wait.',
			'Verifying WebSocket credentials... Please wait.',
			'Authorizing WebSocket session... Please wait.'
		];
		let currentMessageIndex = 0; // Tracks the index of the currently displayed message
		// Set up an interval to rotate through the messages every 100ms
		this.messageInterval = setInterval(() => {
			if (currentMessageIndex < messages.length) {
				// Update the loading status element with the current message
				$('.app-loading-status > small').innerHTML = messages[currentMessageIndex];
				currentMessageIndex++; // Move to the next message
			}
		}, 500);
	},

	handleWebSocketSetup: function () {
		// Fetch and authenticate the JWT using the provided WebSocket auth URL (external JWT)
		if (this.AppWSConn.ws_auth_url) {
			this.fetchAndAuthenticateJWT();
		}
		// If a JWT exists or WebSocket configuration is provided
		else if (window.goaccessJWT || Object.keys(this.AppWSConn).length) {
			// Set up the WebSocket connection using the existing JWT
			this.setWebSocket(this.AppWSConn, window.goaccessJWT, this.messageInterval);
		}  else {
			// Initialize the application without WebSocket authentication
			this.initializeWithoutWebSocket();
		}
	},

	fetchAndAuthenticateJWT: function () {
		// Attempt to fetch a JWT from the WebSocket authentication URL
		this.fetchJWT(this.AppWSConn.ws_auth_url)
			.then(data => {
				if (data.status === "success") {
					// Extract the JWT, refresh token, and expiration time from the response
					const jwt = data.access_token;
					const refreshToken = data.refresh_token;
					const expiresIn = data.expires_in;
					// Set up the WebSocket connection using the fetched JWT
					this.setWebSocket(this.AppWSConn, jwt, this.messageInterval);
					// Schedule automatic token refresh before it expires
					this.scheduleTokenRefresh(expiresIn, refreshToken);
				} else {
					// Handle failure response from the authentication server
					this.handleAuthenticationFailure(data.message);
				}
			})
			.catch(error => {
				// Handle errors during the JWT fetch process
				this.handleAuthenticationError(error);
			});
	},

	initializeWithoutWebSocket: function () {
		// Stop the message rotation interval
		clearInterval(this.messageInterval);
		// Update the UI to indicate that no authentication is provided
		$('.app-loading-status > small').innerHTML = 'No authentication provided.';
		// Proceed to initialize the app without WebSocket support
		GoAccess.App.initialize();
		this.isAppInitialized = true;
	},

	handleAuthenticationFailure: function (message) {
		// Stop the message rotation interval
		clearInterval(this.messageInterval);
		// Update the UI to display the failure message
		$('.app-loading-status > small').innerHTML = `Authentication failed: ${message}`;
		// Hide the loading spinner
		$('.loading-container > .spinner').style.display = 'none';
	},

	handleAuthenticationError: function (error) {
		// Stop the message rotation interval
		clearInterval(this.messageInterval);
		// Update the UI to indicate an error occurred during the JWT fetch process
		$('.app-loading-status > small').innerHTML = 'Error fetching authentication token.';
	},

	getPanelUI: function (panel) {
		return panel ? this.AppUIData[panel] : this.AppUIData;
	},

	getPrefs: function (panel) {
		return panel ? this.AppPrefs[panel] : this.AppPrefs;
	},

	setPrefs: function () {
		if (GoAccess.Util.hasLocalStorage()) {
			localStorage.setItem('AppPrefs', JSON.stringify(GoAccess.getPrefs()));
		}
	},

	getPanelData: function (panel) {
		return panel ? this.AppData[panel] : this.AppData;
	},

	// Include cookies for session validation
	fetchJWT: function (url) {
		return fetch(url, {
			method: 'GET',
			credentials: 'include',
			headers: { 'Accept': 'application/json' },
			referrerPolicy: 'no-referrer-when-downgrade'
		})
		.then(response => response.json())
		.then(data => {
			if (data.status === 'success' && data.csrf_token) {
				this.csrfToken = data.csrf_token;
			}
			return data;
		});
	},

	refreshJWT: function (url, refreshToken) {
		const headers = {
			'Accept': 'application/json',
			'Content-Type': 'application/json'
		};
		if (this.csrfToken) {
			headers['X-CSRF-TOKEN'] = this.csrfToken;
		}
		return fetch(url, {
			method: 'POST',
			credentials: 'include',
			headers: headers,
			referrerPolicy: 'no-referrer-when-downgrade',
			body: JSON.stringify({ refresh_token: refreshToken })
		}).then(response => response.json());
	},

	// Schedule the next token refresh, triggering a refresh shortly before the token expires
	scheduleTokenRefresh: function (expiresIn, refreshToken) {
		// Refresh 1 minute before expiration
		const refreshUrl = this.AppWSConn.ws_auth_refresh_url || this.AppWSConn.ws_auth_url;
		// Set the timer to trigger one minute before the token expires
		setTimeout(() => {
			this.refreshJWT(refreshUrl, refreshToken)
				.then(data => {
					if (data.status === "success") {
						const newJwt = data.access_token;
						const newRefreshToken = data.refresh_token;
						const newExpiresIn = data.expires_in;
						// Update token without reconnecting
						this.sendNewJWT(newJwt);
						// Schedule the next refresh using the new expiration time
						this.scheduleTokenRefresh(newExpiresIn, newRefreshToken);
					} else {
						// Update token without reconnecting
						this.sendNewJWT(null);
					}
				})
				.catch(error => {
					console.error("Error refreshing JWT:", error);
				});
		}, (expiresIn - this.tokenRefreshLeadTime) * 1000);
	},

	// Sends the new JWT to the server over the already-open WebSocket connection
	sendNewJWT: function (newJwt) {
		if (this.socket && this.socket.readyState === WebSocket.OPEN) {
			// Notify the server to update the JWT used for authentication
			this.socket.send(JSON.stringify({ action: "validate_token", token: newJwt }));
		}
		// Also update the locally stored token
		this.currentJWT = newJwt;
	},

	reconnect: function (wsConn) {
		if (this.retries >= this.maxRetries)
			return window.clearTimeout(this.wsTimer);

		this.retries++;
		// Exponential backoff
		if (this.currDelay < this.maxDelay)
			this.currDelay *= 2;
		this.setWebSocket(wsConn, this.currentJWT, null);
	},

	buildWSURI: function (wsConn) {
		var url = null;
		if (!wsConn.url || !wsConn.port)
			return null;
		url = /^wss?:\/\//i.test(wsConn.url) ? wsConn.url : window.location.protocol === "https:" ? 'wss://' + wsConn.url : 'ws://' + wsConn.url;
		return new URL(url).protocol + '//' + new URL(url).hostname + ':' + wsConn.port + new URL(url).pathname;
	},

	setWebSocket: function (wsConn, jwt, messageInterval) {
		var host = null, pingId = null, uri = null, defURI = null, str = null;
		// Store the JWT used for this connection
		this.currentJWT = jwt;

		// If no external messageInterval is provided, set up local message rotation
		if (jwt && !messageInterval) {
			const messages = [
				'Validating WebSocket tokens... Please wait.',
				'Authenticating WebSocket connection... Please wait.',
				'Verifying WebSocket credentials... Please wait.',
				'Authorizing WebSocket session... Please wait.'
			];
			let currentMessageIndex = 0;
			messageInterval = setInterval(() => {
				if (currentMessageIndex < messages.length) {
					$('.app-loading-status > small').innerHTML = messages[currentMessageIndex];
					currentMessageIndex++;
				}
			}, 100);
		}

		defURI = window.location.hostname ? window.location.hostname + ':' + wsConn.port : "localhost" + ':' + wsConn.port;
		uri = wsConn.url && /^(wss?:\/\/)?[^\/]+:[0-9]{1,5}/.test(wsConn.url) ? wsConn.url : this.buildWSURI(wsConn);

		str = uri || defURI;
		str = !/^wss?:\/\//i.test(str) ? (window.location.protocol === "https:" ? 'wss://' : 'ws://') + str : str;

		if (jwt) {
			const separator = str.includes('?') ? '&' : '?';
			str = str + separator + 'token=' + encodeURIComponent(jwt);
		}
		// Store socket for token refresh
		var socket = new WebSocket(str);
		this.socket = socket;

		socket.onopen = function (event) {
			clearInterval(messageInterval);
			if (this.currentJWT)
				$('.app-loading-status > small').innerHTML = 'Authentication successful.';

			this.currDelay = this.wsDelay;
			this.retries = 0;

			if (wsConn.ping_interval) {
				pingId = setInterval(() => { socket.send('ping'); }, wsConn.ping_interval * 1E3);
			}
			GoAccess.Nav.WSOpen(str);

		}.bind(this);

		socket.onmessage = function (event) {
			this.AppState['updated'] = true;
			this.AppData = JSON.parse(event.data);
			if (!this.isAppInitialized) {
				GoAccess.App.initialize();
				GoAccess.Nav.WSOpen(str);
				this.isAppInitialized = true;
			}
			this.App.renderData();
		}.bind(this);

		socket.onclose = function (event) {
			clearInterval(messageInterval);
			$('.app-loading-status > small').innerHTML = 'Unable to authenticate WebSocket.';
			$('.loading-container > .spinner').style.display = 'none';

			GoAccess.Nav.WSClose();
			window.clearInterval(pingId);
			this.socket = null;
			this.wsTimer = setTimeout(() => { this.reconnect(wsConn); }, this.currDelay);
		}.bind(this);
	},
};

// HELPERS
GoAccess.Util = {
	months: ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul","Aug", "Sep", "Oct", "Nov", "Dec"],

	// Add all attributes of n to o
	merge: function (o, n) {
		var obj = {}, i = 0, il = arguments.length, key;
		for (; i < il; i++) {
			for (key in arguments[i]) {
				if (arguments[i].hasOwnProperty(key)) {
					obj[key] = arguments[i][key];
				}
			}
		}
		return obj;
	},

	// hash a string
	hashCode: function (s) {
		return (s.split('').reduce(function (a, b) {
			a = ((a << 5) - a) + b.charCodeAt(0);
			return a&a;
		}, 0) >>> 0).toString(16);
	},

	// Format bytes to human-readable
	formatBytes: function (bytes, decimals, numOnly) {
		if (bytes == 0)
			return numOnly ? 0 : '0 Byte';
		var k = 1024;
		var dm = decimals + 1 || 2;
		var sizes = ['B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB'];
		var i = Math.floor(Math.log(bytes) / Math.log(k));
		return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + (numOnly ? '' : (' ' + sizes[i]));
	},

	// Validate number
	isNumeric: function (n) {
		return !isNaN(parseFloat(n)) && isFinite(n);
	},

	// Format microseconds to human-readable
	utime2str: function (usec) {
		if (usec >= 864E8)
			return ((usec) / 864E8).toFixed(2) + ' d';
		else if (usec >= 36E8)
			return ((usec) / 36E8).toFixed(2) + ' h';
		else if (usec >= 6E7)
			return ((usec) / 6E7).toFixed(2) + ' m';
		else if (usec >= 1E6)
			return ((usec) / 1E6).toFixed(2) + ' s';
		else if (usec >= 1E3)
			return ((usec) / 1E3).toFixed(2) + ' ms';
		return (usec).toFixed(2) + ' us';
	},

	// Format date from 20120124 to 24/Jan/2012
	formatDate: function (str) {
		var y = str.substr(0,4), m = str.substr(4,2) - 1, d = str.substr(6,2),
			h = str.substr(8,2) || 0, i = str.substr(10, 2)  || 0, s = str.substr(12, 2) || 0;
		var date = new Date(y,m,d,h,i,s);

		var out = ('0' + date.getDate()).slice(-2) + '/' + this.months[date.getMonth()] + '/' + date.getFullYear();
		10 <= str.length && (out += ":" + h);
		12 <= str.length && (out += ":" + i);
		14 <= str.length && (out += ":" + s);
		return out;
	},

	shortNum:  function (n) {
		if (n < 1e3) return n;
		if (n >= 1e3 && n < 1e6) return +(n / 1e3).toFixed(1) + "K";
		if (n >= 1e6 && n < 1e9) return +(n / 1e6).toFixed(1) + "M";
		if (n >= 1e9 && n < 1e12) return +(n / 1e9).toFixed(1) + "B";
		if (n >= 1e12) return +(n / 1e12).toFixed(1) + "T";
	},

	// Format field value to human-readable
	fmtValue: function (value, dataType, decimals, shorten, hlregex, hlvalue) {
		var val = 0;
		if (!dataType)
			val = value;

		switch (dataType) {
		case 'utime':
			val = this.utime2str(+value);
			break;
		case 'date':
			val = this.formatDate(value);
			break;
		case 'numeric':
			if (this.isNumeric(value))
				val = shorten ? this.shortNum(value) : (+value).toLocaleString();
			break;
		case 'bytes':
			val = this.formatBytes(value, decimals);
			break;
		case 'percent':
			val = value.replace(',', '.') + '%';
			break;
		case 'time':
			if (this.isNumeric(value))
				val = value.toLocaleString();
			break;
		case 'secs':
			var t = new Date(null);
			t.setSeconds(value);
			val = t.toISOString().substr(11, 8);
			break;
		default:
			val = value;
		}

		if (hlregex) {
			let o = JSON.parse(hlregex), tmp = '';
			for (var x in o) {
				if (!val) continue;
				tmp = val.replace(new RegExp(x, 'gi'), o[x]);
				if (tmp != val) {
					val = tmp;
					break;
				}
				val = tmp;
			}
		}

		return value == 0 ? String(val) : (val === undefined ? '—' : val);
	},

	isPanelHidden: function (panel) {
		return GoAccess.AppPrefs.hiddenPanels.includes(panel);
	},

	isPanelValid: function (panel) {
		var data = GoAccess.getPanelData(), ui = GoAccess.getPanelUI();
		return (!ui.hasOwnProperty(panel) || !data.hasOwnProperty(panel) || !ui[panel].id);
	},

	// Attempts to extract the count from either an object or a scalar.
	// e.g., item = Object {count: 14351, percent: 5.79} OR item = 4824825140
	getCount: function (item) {
		if (this.isObject(item) && 'count' in item)
			return item.count;
		return item;
	},

	getPercent: function (item) {
		if (this.isObject(item) && 'percent' in item)
			return this.fmtValue(item.percent, 'percent');
		return null;
	},

	isObject: function (o) {
		return o === Object(o);
	},

	setProp: function (o, s, v) {
		var schema = o;
		var a = s.split('.');
		for (var i = 0, n = a.length; i < n-1; ++i) {
			var k = a[i];
			if (!schema[k])
				schema[k] = {};
			schema = schema[k];
		}
		schema[a[n-1]] = v;
	},

	getProp: function (o, s) {
		s = s.replace(/\[(\w+)\]/g, '.$1');
		s = s.replace(/^\./, '');
		var a = s.split('.');
		for (var i = 0, n = a.length; i < n; ++i) {
			var k = a[i];
			if (this.isObject(o) && k in o) {
				o = o[k];
			} else {
				return;
			}
		}
		return o;
	},

	hasLocalStorage: function () {
		try {
			localStorage.setItem('test', 'test');
			localStorage.removeItem('test');
			return true;
		} catch(e) {
			return false;
		}
	},

	isWithinViewPort: function (el) {
		var elemTop = el.getBoundingClientRect().top;
		var elemBottom = el.getBoundingClientRect().bottom;
		return elemTop < window.innerHeight && elemBottom >= 0;
	},

	togglePanel: function(panel) {
		var index = GoAccess.AppPrefs.hiddenPanels.indexOf(panel);
		if (index == -1) {
			GoAccess.AppPrefs.hiddenPanels.push(panel);
		} else {
			GoAccess.AppPrefs.hiddenPanels.splice(index, 1);
		}
		GoAccess.setPrefs();

		delete GoAccess.AppCharts[panel];
		GoAccess.OverallStats.initialize();
		GoAccess.Panels.initialize();
		GoAccess.Charts.initialize();
		GoAccess.Tables.initialize();
	},

	reorderPanels: function(fromIndex, toIndex) {
		var order = GoAccess.AppPrefs.panelOrder;

		// Ensure we have a valid order array
		if (!order || order.length === 0) {
			console.error('Panel order not initialized');
			return;
		}

		// Validate indices
		if (fromIndex < 0 || fromIndex >= order.length ||
			toIndex < 0 || toIndex >= order.length) {
			console.error('Invalid drag indices', fromIndex, toIndex);
			return;
		}

		// Perform the reorder
		var item = order.splice(fromIndex, 1)[0];
		order.splice(toIndex, 0, item);

		// Save preferences
		GoAccess.setPrefs();

		// Re-render panels in new order
		GoAccess.Panels.initialize();
		GoAccess.Charts.initialize();
		GoAccess.Tables.initialize();
	},
};

// OVERALL STATS
GoAccess.OverallStats = {
	total_requests: 0,

	// Render each overall stats box
	renderBox: function (data, ui, row, x, idx) {
		var wrap = $('#overall ul');
		var box = document.createElement('li');

		// we need to append the element first, otherwise outerHTML won't work
		wrap.appendChild(box);

		box.outerHTML = GoAccess.AppTpls.General.items.render({
			'id': x,
			'className': ui.items[x].className,
			'label': ui.items[x].label,
			'value': GoAccess.Util.fmtValue(data[x], ui.items[x].dataType),
		});

		return wrap;
	},

	// Render overall stats
	renderData: function (data, ui) {
		var idx = 0, row = null;

		$('.last-updated').innerHTML = data.date_time;
		$('#overall').innerHTML = '';

		if (GoAccess.Util.isPanelHidden('general'))
			return false;

		$('#overall').innerHTML = GoAccess.AppTpls.General.wrap.render(GoAccess.Util.merge(ui, {
			'from': data.start_date,
			'to': data.end_date,
		}));
		$('#overall').setAttribute('aria-labelledby', 'overall-heading');

		// Iterate over general data object
		for (var x in data) {
			if (!data.hasOwnProperty(x) || !ui.items.hasOwnProperty(x))
				continue;
			row = this.renderBox(data, ui, row, x, idx);
			idx++;
		}
	},

	// Render general/overall analyzed requests.
	initialize: function () {
		var ui = GoAccess.getPanelUI('general');
		var data = GoAccess.getPanelData('general');
		this.total_requests = data.total_requests;

		this.renderData(data, ui);
	}
};

// RENDER PANELS
GoAccess.Nav = {
	events: function () {
		$('.nav-bars').onclick = function (e) {
			e.stopPropagation();
			this.renderMenu(e);
		}.bind(this);

		$('.nav-gears').onclick = function (e) {
			e.stopPropagation();
			this.renderOpts(e);
		}.bind(this);

		$('.nav-minibars').onclick = function (e) {
			e.stopPropagation();
			this.renderOpts(e);
		}.bind(this);

		$('body').onclick = function (e) {
			$('nav').classList.remove('active');
		}.bind(this);

		$$('.export-json', function (item) {
			item.onclick = function (e) {
				this.downloadJSON(e);
			}.bind(this);
		}.bind(this));

		$$('.theme-bright', function (item) {
			item.onclick = function (e) {
				this.setTheme('bright');
			}.bind(this);
		}.bind(this));

		$$('.theme-dark-blue', function (item) {
			item.onclick = function (e) {
				this.setTheme('darkBlue');
			}.bind(this);
		}.bind(this));

		$$('.theme-dark-gray', function (item) {
			item.onclick = function (e) {
				this.setTheme('darkGray');
			}.bind(this);
		}.bind(this));

		$$('.theme-dark-purple', function (item) {
			item.onclick = function (e) {
				this.setTheme('darkPurple');
			}.bind(this);
		}.bind(this));

		$$('.layout-horizontal', function (item) {
			item.onclick = function (e) {
				this.setLayout('horizontal');
			}.bind(this);
		}.bind(this));

		$$('.layout-vertical', function (item) {
			item.onclick = function (e) {
				this.setLayout('vertical');
			}.bind(this);
		}.bind(this));

		$$('.layout-wide', function (item) {
			item.onclick = function (e) {
				this.setLayout('wide');
			}.bind(this);
		}.bind(this));

		$$('[data-perpage]', function (item) {
			item.onclick = function (e) {
				this.setPerPage(e);
			}.bind(this);
		}.bind(this));

		$$('[data-show-tables]', function (item) {
			item.onclick = function (e) {
				this.toggleTables();
			}.bind(this);
		}.bind(this));

		$$('[data-autohide-tables]', function (item) {
			item.onclick = function (e) {
				this.toggleAutoHideTables();
			}.bind(this);
		}.bind(this));

		$$('.toggle-panel', function (item) {
			item.onclick = function (e) {
				e.stopPropagation();
				var panel = e.currentTarget.getAttribute('data-panel');
				GoAccess.Util.togglePanel(panel);
				item.classList.toggle('active');
			}.bind(this);
		}.bind(this));

		$$('.drag-handle', function (item) {
			var li = item.closest('li');

			// Don't make the overall stats draggable
			var link = li.querySelector('a');
			if (link && link.getAttribute('href') === '#') {
				return; // Skip overall stats item
			}

			li.setAttribute('draggable', 'true');

			li.ondragstart = function(e) {
				e.dataTransfer.effectAllowed = 'move';
				e.dataTransfer.setData('text/html', this.innerHTML);
				this.classList.add('dragging');

				// Get the actual panel key from the link
				var panelLink = this.querySelector('a');
				var panelKey = panelLink ? panelLink.getAttribute('href').substring(1) : '';
				e.dataTransfer.setData('panelKey', panelKey);

				// Store the index in the ordered list (excluding overall)
				var allItems = Array.from(this.parentNode.children);
				var draggableItems = allItems.filter(function(item) {
					var itemLink = item.querySelector('a');
					return itemLink && itemLink.getAttribute('href') !== '#';
				});
				var fromIndex = draggableItems.indexOf(this);
				e.dataTransfer.setData('index', fromIndex);
			};

			li.ondragend = function(e) {
				this.classList.remove('dragging');
				$$('.nav-list li', function(item) {
					item.classList.remove('drag-over');
				});
			};

			li.ondragover = function(e) {
				e.preventDefault();
				e.dataTransfer.dropEffect = 'move';

				// Only allow drop on other draggable items
				var link = this.querySelector('a');
				if (link && link.getAttribute('href') !== '#') {
					return false;
				}
				return true;
			};

			li.ondragenter = function(e) {
				e.preventDefault();
				// Only highlight if it's a valid drop target
				var link = this.querySelector('a');
				if (link && link.getAttribute('href') !== '#') {
					this.classList.add('drag-over');
				}
			};

			li.ondragleave = function(e) {
				// Check if we're actually leaving the element (not just entering a child)
				if (e.target === this) {
					this.classList.remove('drag-over');
				}
			};

			li.ondrop = function(e) {
				e.stopPropagation();
				e.preventDefault();

				// Don't allow dropping on overall stats
				var targetLink = this.querySelector('a');
				if (targetLink && targetLink.getAttribute('href') === '#') {
					return false;
				}

				var fromIndex = parseInt(e.dataTransfer.getData('index'));

				// Calculate toIndex from draggable items only
				var allItems = Array.from(this.parentNode.children);
				var draggableItems = allItems.filter(function(item) {
					var itemLink = item.querySelector('a');
					return itemLink && itemLink.getAttribute('href') !== '#';
				});
				var toIndex = draggableItems.indexOf(this);

				if (fromIndex !== toIndex && fromIndex !== -1 && toIndex !== -1) {
					GoAccess.Util.reorderPanels(fromIndex, toIndex);
					// Re-render the menu to show new order
					GoAccess.Nav.renderMenu();
				}

				this.classList.remove('drag-over');
				return false;
			};
		}.bind(this));
	},

	downloadJSON: function (e) {
		var targ = e.currentTarget;
		var data = "text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(GoAccess.getPanelData()));
		targ.href = 'data:' + data;
		targ.download = 'goaccess-' + (+new Date()) + '.json';
	},

	setLayout: function (layout) {
		if (('horizontal' == layout || 'wide' == layout) && $('.container')) {
			$('.container').classList.add('container-fluid');
			$('.container').classList.remove('container');
		} else if ('vertical' == layout && $('.container-fluid')) {
			$('.container-fluid').classList.add('container');
			$('.container').classList.remove('container-fluid');
		}

		GoAccess.AppPrefs['layout'] = layout;
		GoAccess.setPrefs();

		GoAccess.Panels.initialize();
		GoAccess.Charts.initialize();
		GoAccess.Tables.initialize();
	},

	toggleAutoHideTables: function (e) {
		var autoHideTables = GoAccess.Tables.autoHideTables();
		$$('.table-wrapper', function (item) {
			if (autoHideTables) {
				item.classList.remove('hidden-xs');
			} else {
				item.classList.add('hidden-xs');
			}
		}.bind(this));

		GoAccess.AppPrefs['autoHideTables'] = !autoHideTables;
		GoAccess.setPrefs();
	},

	toggleTables: function () {
		var ui = GoAccess.getPanelUI();
		var showTables = GoAccess.Tables.showTables();
		Object.keys(ui).forEach(function (panel, idx) {
			if (!GoAccess.Util.isPanelValid(panel) || GoAccess.Util.isPanelHidden(panel))
				ui[panel]['table'] = !showTables;
		}.bind(this));

		GoAccess.AppPrefs['showTables'] = !showTables;
		GoAccess.setPrefs();

		GoAccess.Panels.initialize();
		GoAccess.Charts.initialize();
		GoAccess.Tables.initialize();
	},

	setTheme: function (theme) {
		if (!theme)
			return;

		$('html').className = '';
		switch(theme) {
		case 'darkGray':
			document.querySelector('meta[name="theme-color"]')?.setAttribute('content', '#212121');
			$('html').classList.add('dark');
			$('html').classList.add('gray');
			break;
		case 'darkBlue':
			document.querySelector('meta[name="theme-color"]')?.setAttribute('content', '#252B30');
			$('html').classList.add('dark');
			$('html').classList.add('blue');
			break;
		case 'darkPurple':
			document.querySelector('meta[name="theme-color"]')?.setAttribute('content', '#1e1e2f');
			$('html').classList.add('dark');
			$('html').classList.add('purple');
			break;
		default:
			document.querySelector('meta[name="theme-color"]')?.setAttribute('content', '#f0f0f0');
		}
		GoAccess.AppPrefs['theme'] = theme;
		GoAccess.setPrefs();
	},

	getIcon: function (key) {
		switch(key) {
		case 'visitors'        : return 'users';
		case 'requests'        : return 'file';
		case 'static_requests' : return 'file-text';
		case 'not_found'       : return 'file-o';
		case 'hosts'           : return 'user';
		case 'os'              : return 'desktop';
		case 'browsers'        : return 'chrome';
		case 'visit_time'      : return 'clock-o';
		case 'vhosts'          : return 'th-list';
		case 'referrers'       : return 'external-link';
		case 'referring_sites' : return 'external-link';
		case 'keyphrases'      : return 'google';
		case 'status_codes'    : return 'warning';
		case 'remote_user'     : return 'users';
		case 'geolocation'     : return 'map-marker';
		case 'asn'             : return 'map-marker';
		case 'mime_type'       : return 'file-o';
		case 'tls_type'        : return 'warning';
		default                : return 'pie-chart';
		}
	},

	getItems: function () {
		var ui = GoAccess.getPanelUI(), menu = [], panels = [];

		// Collect all valid panels
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel))
				continue;
			panels.push({
				'current': window.location.hash.substr(1) == panel,
				'head': ui[panel].head,
				'key': panel,
				'icon': this.getIcon(panel),
				'hidden': GoAccess.Util.isPanelHidden(panel)
			});
		}

		// Initialize panel order if empty
		if (!GoAccess.AppPrefs.panelOrder || GoAccess.AppPrefs.panelOrder.length === 0) {
			GoAccess.AppPrefs.panelOrder = panels.map(function(p) { return p.key; });
			GoAccess.setPrefs();
		}

		// Sort panels according to saved order
		var orderedPanels = [];
		var order = GoAccess.AppPrefs.panelOrder;

		// First add panels in the saved order
		for (var i = 0; i < order.length; i++) {
			var panel = panels.find(function(p) { return p.key === order[i]; });
			if (panel) orderedPanels.push(panel);
		}

		// Then add any new panels that aren't in the saved order
		for (var j = 0; j < panels.length; j++) {
			if (!order.includes(panels[j].key)) {
				orderedPanels.push(panels[j]);
				GoAccess.AppPrefs.panelOrder.push(panels[j].key);
			}
		}

		return orderedPanels;
	},

	setPerPage: function (e) {
		GoAccess.AppPrefs['perPage'] = +e.currentTarget.getAttribute('data-perpage');
		GoAccess.App.renderData();
		GoAccess.setPrefs();

		GoAccess.Tables.initialize();
	},

	getTheme: function () {
		return GoAccess.AppPrefs.theme || 'darkGray';
	},

	getLayout: function () {
		return GoAccess.AppPrefs.layout || 'horizontal';
	},

	getPerPage: function () {
		return GoAccess.AppPrefs.perPage || 7;
	},

	// Render left-hand side navigation options.
	renderOpts: function () {
		var o = {};
		o[this.getLayout()] = true;
		o[this.getTheme()] = true;
		o['perPage' + this.getPerPage()] = true;
		o['autoHideTables'] = GoAccess.Tables.autoHideTables();
		o['showTables'] = GoAccess.Tables.showTables();
		o['labels'] = GoAccess.i18n;

		$('.nav-list').innerHTML = GoAccess.AppTpls.Nav.opts.render(o);
		requestAnimationFrame(function () {
			$('nav').classList.toggle('active');
		});
		this.events();
	},

	// Render left-hand side navigation given the available panels.
	renderMenu: function (e) {
		$('.nav-list').innerHTML = GoAccess.AppTpls.Nav.menu.render({
			'nav': this.getItems(),
			'overall_current': window.location.hash.substr(1) == '',
			'overall_hidden': GoAccess.Util.isPanelHidden('general'),
			'labels': GoAccess.i18n,
		});
		requestAnimationFrame(function () {
			$('nav').classList.toggle('active');
		});
		this.events();
	},

	WSStatus: function () {
		if (Object.keys(GoAccess.AppWSConn).length)
			$$('.nav-ws-status', function (item) { item.style.display = 'block'; });
	},

	WSClose: function () {
		$$('.nav-ws-status', function (item) {
			item.classList.remove('fa-circle');
			item.classList.add('fa-stop');
			item.setAttribute('aria-label', GoAccess.i18n.websocket_disconnected);
			item.setAttribute('title', GoAccess.i18n.websocket_disconnected);
		});
	},

	WSOpen: function (str) {
		const baseUrl = str.split('?')[0].split('#')[0];
		$$('.nav-ws-status', function (item) {
			item.classList.remove('fa-stop');
			item.classList.add('fa-circle');
			item.setAttribute('aria-label', `${GoAccess.i18n.websocket_connected} (${baseUrl})`);
			item.setAttribute('title', `${GoAccess.i18n.websocket_connected} (${baseUrl})`);
		});
	},

	// Render left-hand side navigation given the available panels.
	renderWrap: function (nav) {
		$('nav').innerHTML = GoAccess.AppTpls.Nav.wrap.render(GoAccess.i18n);
	},

	// Iterate over all available panels and render each.
	initialize: function () {
		this.setTheme(GoAccess.AppPrefs.theme);
		this.renderWrap();
		this.WSStatus();
		this.events();
	}
};

// RENDER PANELS
GoAccess.Panels = {
	events: function () {
		$$('[data-toggle=dropdown]', function (item) {
			item.onclick = function (e) {
				this.openOpts(e.currentTarget);
			}.bind(this);
			item.onblur = function (e) {
				this.closeOpts(e);
			}.bind(this);
		}.bind(this));

		$$('[data-plot]', function (item) {
			item.onclick = function (e) {
				GoAccess.Charts.redrawChart(e.currentTarget);
			}.bind(this);
		}.bind(this));

		$$('[data-chart]', function (item) {
			item.onclick = function (e) {
				GoAccess.Charts.toggleChart(e.currentTarget);
			}.bind(this);
		}.bind(this));

		$$('[data-chart-type]', function (item) {
			item.onclick = function (e) {
				GoAccess.Charts.setChartType(e.currentTarget);
			}.bind(this);
		}.bind(this));

		$$('[data-metric]', function (item) {
			item.onclick = function (e) {
				GoAccess.Tables.toggleColumn(e.currentTarget);
			}.bind(this);
		}.bind(this));
	},

	openOpts: function (targ) {
		var panel = targ.getAttribute('data-panel');
    targ.setAttribute('aria-expanded', 'true');
		targ.parentElement.classList.toggle('open');
		this.renderOpts(panel);
	},

	closeOpts: function (e) {
		e.currentTarget.parentElement.classList.remove('open');
    e.currentTarget.parentElement.querySelector('[aria-expanded]').setAttribute('aria-expanded', 'false');
		// Trigger the click event on the target if not opening another menu
		if (e.relatedTarget && e.relatedTarget.getAttribute('data-toggle') !== 'dropdown')
			e.relatedTarget.click();
	},

	setPlotSelection: function (ui, prefs) {
		var chartType = ((prefs || {}).plot || {}).chartType || ui.plot[0].chartType;
		var metric = ((prefs || {}).plot || {}).metric || ui.plot[0].className;

		ui[chartType] = true;
		for (var i = 0, len = ui.plot.length; i < len; ++i)
			if (ui.plot[i].className == metric)
				ui.plot[i]['selected'] = true;
	},

	setColSelection: function (items, prefs) {
		var columns = (prefs || {}).columns || {};
		for (var i = 0, len = items.length; i < len; ++i)
			if ((items[i].key in columns) && columns[items[i].key]['hide'])
				items[i]['hide'] = true;
	},

	setChartSelection: function (ui, prefs) {
		ui['showChart'] = prefs && ('chart' in prefs) ? prefs.chart : true;
	},

	setOpts: function (panel) {
		var ui = JSON.parse(JSON.stringify(GoAccess.getPanelUI(panel))), prefs = GoAccess.getPrefs(panel);
		// set preferences selection upon opening panel options
		this.setChartSelection(ui, prefs);
		this.setPlotSelection(ui, prefs);
		this.setColSelection(ui.items, prefs);
		return GoAccess.Util.merge(ui, {'labels': GoAccess.i18n});
	},

	renderOpts: function (panel) {
		$('.panel-opts-' + panel).innerHTML = GoAccess.AppTpls.Panels.opts.render(this.setOpts(panel));
		this.events();
	},

	enablePrev: function (panel) {
		var $pagination = $('#panel-' + panel + ' .pagination a.panel-prev');
		if ($pagination)
			$pagination.parentNode.classList.remove('disabled');
	},

	disablePrev: function (panel) {
		var $pagination = $('#panel-' + panel + ' .pagination a.panel-prev');
		if ($pagination)
			$pagination.parentNode.classList.add('disabled');
	},

	enableNext: function (panel) {
		var $pagination = $('#panel-' + panel + ' .pagination a.panel-next');
		if ($pagination)
			$pagination.parentNode.classList.remove('disabled');
	},

	disableNext: function (panel) {
		var $pagination = $('#panel-' + panel + ' .pagination a.panel-next');
		if ($pagination)
			$pagination.parentNode.classList.add('disabled');
	},

	enableFirst: function (panel) {
		var $pagination = $('#panel-' + panel + ' .pagination a.panel-first');
		if ($pagination)
			$pagination.parentNode.classList.remove('disabled');
	},

	disableFirst: function (panel) {
		var $pagination = $('#panel-' + panel + ' .pagination a.panel-first');
		if ($pagination)
			$pagination.parentNode.classList.add('disabled');
	},

	enableLast: function (panel) {
		var $pagination = $('#panel-' + panel + ' .pagination a.panel-last');
		if ($pagination)
			$pagination.parentNode.classList.remove('disabled');
	},

	disableLast: function (panel) {
		var $pagination = $('#panel-' + panel + ' .pagination a.panel-last');
		if ($pagination)
			$pagination.parentNode.classList.add('disabled');
	},

	enablePagination: function (panel) {
		this.enablePrev(panel);
		this.enableNext(panel);
		this.enableFirst(panel);
		this.enableLast(panel);
	},

	disablePagination: function (panel) {
		this.disablePrev(panel);
		this.disableNext(panel);
		this.disableFirst(panel);
		this.disableLast(panel);
	},

	hasSubItems: function (ui, data) {
		for (var i = 0, len = data.length; i < len; ++i) {
			if (!data[i].items)
				return (ui['hasSubItems'] = false);
			if (data[i].items.length) {
				return (ui['hasSubItems'] = true);
			}
		}
		return false;
	},

	setComputedData: function (panel, ui, data) {
		this.hasSubItems(ui, data.data);
		GoAccess.Charts.hasChart(panel, ui);
		GoAccess.Tables.hasTable(ui);
	},

	// Render the given panel given a user interface definition.
	renderPanel: function (panel, ui, col) {
		// set some computed values before rendering panel structure
		var data = GoAccess.getPanelData(panel);
		this.setComputedData(panel, ui, data);

		// per panel wrapper
		var box = document.createElement('div');
		box.id = 'panel-' + panel;
		box.innerHTML = GoAccess.AppTpls.Panels.wrap.render(GoAccess.Util.merge(ui, {
			'labels': GoAccess.i18n
		}));

		// add accessible label to parent article
		col.setAttribute('aria-labelledby', panel);

		col.appendChild(box);

		// Remove pagination if not enough data for the given panel
		if (data.data.length <= GoAccess.getPrefs().perPage)
			this.disablePagination(panel);
		GoAccess.Tables.renderThead(panel, ui);

		return col;
	},

	createCol: function (row) {
		var layout = GoAccess.AppPrefs['layout'];
		var perRow = 'horizontal' == layout ? 6 : 'wide' == layout ? 3 : 12;

		// set the number of columns based on current layout
		var col = document.createElement('article');
		col.setAttribute('class', 'col-md-' + perRow);
		row.appendChild(col);

		return col;
	},

	createRow: function (row, idx) {
		var wrap = $('#panels');
		var layout = GoAccess.AppPrefs['layout'];
		var every = 'horizontal' == layout ? 2 : 'wide' == layout ? 4 : 1;

		// create a new bootstrap row every one or two elements depending on
		// the layout
		if (idx % every == 0) {
			row = document.createElement('div');
			row.setAttribute('class', 'row' + (every == 2 || every == 4 ? ' equal' : ''));
			wrap.appendChild(row);
		}

		return row;
	},

	resetPanel: function (panel) {
		var ui = GoAccess.getPanelUI();
		var ele = $('#panel-' + panel);

		if (GoAccess.Util.isPanelValid(panel) || GoAccess.Util.isPanelHidden(panel))
			return false;

		var col = ele.parentNode;
		col.removeChild(ele);
		// Render panel given a user interface definition
		this.renderPanel(panel, ui[panel], col);
		this.events();
	},

	// Iterate over all available panels and render each panel
	// structure.
	renderPanels: function () {
		var ui = GoAccess.getPanelUI(), idx = 0, row = null, col = null;
		var order = GoAccess.AppPrefs.panelOrder || [];

		$('#panels').innerHTML = '';

		// If no order is set, create default order
		if (order.length === 0) {
			for (var panel in ui) {
				if (!GoAccess.Util.isPanelValid(panel) && !GoAccess.Util.isPanelHidden(panel)) {
					order.push(panel);
				}
			}
			GoAccess.AppPrefs.panelOrder = order;
			GoAccess.setPrefs();
		}

		// Render panels in the specified order
		for (var i = 0; i < order.length; i++) {
			var panel = order[i];
			if (GoAccess.Util.isPanelValid(panel) || GoAccess.Util.isPanelHidden(panel))
				continue;

			row = this.createRow(row, idx++);
			col = this.createCol(row);
			this.renderPanel(panel, ui[panel], col);
		}

		// Render any new panels not in the order array
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel) || GoAccess.Util.isPanelHidden(panel))
				continue;
			if (!order.includes(panel)) {
				row = this.createRow(row, idx++);
				col = this.createCol(row);
				this.renderPanel(panel, ui[panel], col);
				order.push(panel);
			}
		}

		GoAccess.AppPrefs.panelOrder = order;
		GoAccess.setPrefs();
	},

	initialize: function () {
		this.renderPanels();
		this.events();
	}
};

// RENDER CHARTS
GoAccess.Charts = {
	iter: function (callback) {
		Object.keys(GoAccess.AppCharts).forEach(function (panel) {
			// redraw chart only if it's within the viewport
			if (!GoAccess.Util.isWithinViewPort($('#panel-' + panel)))
				return;
			if (callback && typeof callback === 'function')
				callback.call(this, GoAccess.AppCharts[panel], panel);
		});
	},

	getMetricKeys: function (panel, key) {
		return GoAccess.getPanelUI(panel)['items'].map(function (a) { return a[key]; });
	},

	getPanelData: function (panel, data) {
		// Grab ui plot data for the selected panel
		var plot = GoAccess.Util.getProp(GoAccess.AppState, panel + '.plot');

		// Grab the data for the selected panel
		data = data || this.processChartData(GoAccess.getPanelData(panel).data);
		return plot.chartReverse ? data.reverse() : data;
	},

	drawPlot: function (panel, plotUI, data) {
		var chart = this.getChart(panel, plotUI, data);
		if (!chart)
			return;

		this.renderChart(panel, chart, data);
		GoAccess.AppCharts[panel] = null;
		GoAccess.AppCharts[panel] = chart;
	},

	setChartType: function (targ) {
		var panel = targ.getAttribute('data-panel');
		var type = targ.getAttribute('data-chart-type');

		GoAccess.Util.setProp(GoAccess.AppPrefs, panel + '.plot.chartType', type);
		GoAccess.setPrefs();

		var plotUI = GoAccess.Util.getProp(GoAccess.AppState, panel + '.plot');
		// Extract data for the selected panel and process it
		this.drawPlot(panel, plotUI, this.getPanelData(panel));
	},

	toggleChart: function (targ) {
		var panel = targ.getAttribute('data-panel');
		var prefs = GoAccess.getPrefs(panel),
			chart = prefs && ('chart' in prefs) ? prefs.chart : true;

		GoAccess.Util.setProp(GoAccess.AppPrefs, panel + '.chart', !chart);
		GoAccess.setPrefs();

		GoAccess.Panels.resetPanel(panel);
		GoAccess.Charts.resetChart(panel);
		GoAccess.Tables.renderFullTable(panel);
	},

	hasChart: function (panel, ui) {
		var prefs = GoAccess.getPrefs(panel),
			chart = prefs && ('chart' in prefs) ? prefs.chart : true;
		ui['chart'] = ui.plot.length && chart && chart;
	},

	// Redraw a chart upon selecting a metric.
	redrawChart: function (targ) {
		var plot = targ.getAttribute('data-plot');
		var panel = targ.getAttribute('data-panel');
		var ui = GoAccess.getPanelUI(panel);
		var plotUI = ui.plot;

		GoAccess.Util.setProp(GoAccess.AppPrefs, panel + '.plot.metric', plot);
		GoAccess.setPrefs();

		// Iterate over plot user interface definition
		for (var x in plotUI) {
			if (!plotUI.hasOwnProperty(x) || plotUI[x].className != plot)
				continue;

			GoAccess.Util.setProp(GoAccess.AppState, panel + '.plot', plotUI[x]);
			// Extract data for the selected panel and process it
			this.drawPlot(panel, plotUI[x], this.getPanelData(panel));
			break;
		}
	},

	// Iterate over the item properties and extract the count value.
	extractCount: function (item) {
		var o = {};
		for (var prop in item)
			o[prop] = GoAccess.Util.getCount(item[prop]);
		return o;
	},

	// Extract an array of objects that D3 can consume to process the chart.
	// e.g., o = Object {hits: 37402, visitors: 6949, bytes:
	// 505881789, avgts: 118609, cumts: 4436224010…}
	processChartData: function (data) {
		var out = [];
		for (var i = 0; i < data.length; ++i)
			out.push(this.extractCount(data[i]));
		return out;
	},

	findUIItem: function (panel, key) {
		var items = GoAccess.getPanelUI(panel).items;
		for (var i = 0; i < items.length; ++i) {
			if (items[i].key == key)
				return items[i];
		}
		return null;
	},

	getXKey: function (datum, key) {
		var arr = [];
		if (typeof key === 'string')
			return datum[key];
		for (var prop in key)
			arr.push(datum[key[prop]]);
		return arr.join(' ');
	},

	getWMap: function (panel, plotUI, data, projectionType) {
		var chart = WorldMap(d3.select("#chart-" + panel));
		chart.width($("#chart-" + panel).getBoundingClientRect().width);
		chart.height(400);
		chart.metric(plotUI['d3']['y0']['key']);
		chart.opts(plotUI);
		chart.projectionType(projectionType == 'wmap' ? 'mercator' : 'orthographic');

		return chart;
	},

	getAreaSpline: function (panel, plotUI, data) {
		var dualYaxis = plotUI['d3']['y1'];

		var chart = AreaChart(dualYaxis)
		.labels({
			y0: plotUI['d3']['y0'].label,
			y1: dualYaxis ? plotUI['d3']['y1'].label : ''
		})
		.x(function (d) {
			if ((((plotUI || {}).d3 || {}).x || {}).key)
				return this.getXKey(d, plotUI['d3']['x']['key']);
			return d.data;
		}.bind(this))
		.y0(function (d) {
			return +d[plotUI['d3']['y0']['key']];
		})
		.width($("#chart-" + panel).getBoundingClientRect().width)
		.height(175)
		.format({
			x: (this.findUIItem(panel, 'data') || {}).dataType || null,
			y0: ((plotUI.d3 || {}).y0 || {}).format,
			y1: ((plotUI.d3 || {}).y1 || {}).format,
		})
		.opts(plotUI);

		dualYaxis && chart.y1(function (d) {
			return +d[plotUI['d3']['y1']['key']];
		});

		return chart;
	},

	getVBar: function (panel, plotUI, data) {
		var dualYaxis = plotUI['d3']['y1'];

		var chart = BarChart(dualYaxis)
		.labels({
			y0: plotUI['d3']['y0'].label,
			y1: dualYaxis ? plotUI['d3']['y1'].label : ''
		})
		.x(function (d) {
			if ((((plotUI || {}).d3 || {}).x || {}).key)
				return this.getXKey(d, plotUI['d3']['x']['key']);
			return d.data;
		}.bind(this))
		.y0(function (d) {
			return +d[plotUI['d3']['y0']['key']];
		})
		.width($("#chart-" + panel).getBoundingClientRect().width)
		.height(175)
		.format({
			x: (this.findUIItem(panel, 'data') || {}).dataType || null,
			y0: ((plotUI.d3 || {}).y0 || {}).format,
			y1: ((plotUI.d3 || {}).y1 || {}).format,
		})
		.opts(plotUI);

		dualYaxis && chart.y1(function (d) {
			return +d[plotUI['d3']['y1']['key']];
		});

		return chart;
	},

	getChartType: function (panel) {
		var ui = GoAccess.getPanelUI(panel);
		if (!ui.chart)
			return '';

		return GoAccess.Util.getProp(GoAccess.getPrefs(), panel + '.plot.chartType') || ui.plot[0].chartType;
	},

	getPlotUI: function (panel, ui) {
		var metric = GoAccess.Util.getProp(GoAccess.getPrefs(), panel + '.plot.metric');
		if (!metric)
			return ui.plot[0];
		return ui.plot.filter(function (v) {
			return v.className == metric;
		})[0];
	},

	getChart: function (panel, plotUI, data) {
		var chart = null;

		// Render given its type
		switch (this.getChartType(panel)) {
		case 'area-spline':
			chart = this.getAreaSpline(panel, plotUI, data);
			break;
		case 'bar':
			chart = this.getVBar(panel, plotUI, data);
			break;
		case 'wmap':
		case 'gmap':
			chart = this.getWMap(panel, plotUI, data, this.getChartType(panel));
			break;
		}

		return chart;
	},

	renderChart: function (panel, chart, data) {
		// remove popup
		d3.select('#chart-' + panel + '>.chart-tooltip-wrap')
			.remove();
		// remove svg
		d3.select('#chart-' + panel).selectAll('svg')
			.remove();
		// add chart to the document
		d3.select("#chart-" + panel)
			.datum(data)
			.call(chart)
			.append("div").attr("class", "chart-tooltip-wrap");
	},

	addChart: function (panel, ui) {
		var plotUI = null, chart = null;

		// Ensure it has a plot definition
		if (!ui.plot || !ui.plot.length)
			return;

		plotUI = this.getPlotUI(panel, ui);
		// set ui plot data
		GoAccess.Util.setProp(GoAccess.AppState, panel + '.plot', plotUI);

		// Grab the data for the selected panel
		var data = this.getPanelData(panel);
		if (!(chart = this.getChart(panel, plotUI, data)))
			return;

		this.renderChart(panel, chart, data);
		GoAccess.AppCharts[panel] = chart;
	},

	// Render all charts for the applicable panels.
	renderCharts: function (ui) {
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel) || GoAccess.Util.isPanelHidden(panel))
				continue;
			this.addChart(panel, ui[panel]);
		}
	},

	resetChart: function (panel) {
		var ui = {};
		if (GoAccess.Util.isPanelValid(panel) || GoAccess.Util.isPanelHidden(panel))
			return false;

		ui = GoAccess.getPanelUI(panel);
		this.addChart(panel, ui);
	},

	// Reload (doesn't redraw) the given chart's data
	reloadChart: function (chart, panel) {
		var subItems = GoAccess.Tables.getSubItemsData(panel);
		var data = (subItems.length ? subItems : GoAccess.getPanelData(panel).data).slice(0);

		d3.select("#chart-" + panel)
			.datum(this.processChartData(this.getPanelData(panel, data)))
			.call(chart.width($("#chart-" + panel).offsetWidth))
			.append("div").attr("class", "chart-tooltip-wrap");
	},

	// Reload (doesn't redraw) all chart's data
	reloadCharts: function () {
		this.iter(function (chart, panel) {
			this.reloadChart(chart, panel);
		}.bind(this));
		GoAccess.AppState.updated = false;
	},

	// Only redraw charts with current data
	redrawCharts: function () {
		this.iter(function (chart, panel) {
			d3.select("#chart-" + panel).call(chart.width($("#chart-" + panel).offsetWidth));
		});
	},

	initialize: function () {
		this.renderCharts(GoAccess.getPanelUI());

		// reload on scroll & redraw on resize
		d3.select(window).on('scroll.charts', debounce(function () {
			this.reloadCharts();
		}, 250, false).bind(this)).on('resize.charts', function () {
			this.redrawCharts();
		}.bind(this));
	}
};

// RENDER TABLES
GoAccess.Tables = {
	chartData: {}, // holds all panel sub items data that feeds the chart

	events: function () {
		$$('.panel-next', function (item) {
			item.onclick = function (e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				this.renderTable(panel, this.nextPage(panel));
			}.bind(this);
		}.bind(this));

		$$('.panel-prev', function (item) {
			item.onclick = function (e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				this.renderTable(panel, this.prevPage(panel));
			}.bind(this);
		}.bind(this));

		$$('.panel-first', function (item) {
			item.onclick = function (e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				this.renderTable(panel, "FIRST_PAGE");
			}.bind(this);
		}.bind(this));

		$$('.panel-last', function (item) {
			item.onclick = function (e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				this.renderTable(panel, "LAST_PAGE");
			}.bind(this);
		}.bind(this));

		$$('.expandable>td', function (item) {
			item.onclick = function (e) {
				if (!window.getSelection().toString())
					this.toggleRow(e.currentTarget);
			}.bind(this);
		}.bind(this));

		$$('.row-expandable.clickable', function (item) {
			item.onclick = function (e) {
				this.toggleRow(e.currentTarget);
			}.bind(this);
		}.bind(this));

		$$('.sortable', function (item) {
			item.onclick = function (e) {
				this.sortColumn(e.currentTarget);
			}.bind(this);
		}.bind(this));
	},

	toggleColumn: function (targ) {
		var panel = targ.getAttribute('data-panel');
		var metric = targ.getAttribute('data-metric');

		var columns = (GoAccess.getPrefs(panel) || {}).columns || {};
		if (metric in columns) {
			delete columns[metric];
		} else {
			GoAccess.Util.setProp(columns, metric + '.hide', true);
		}

		GoAccess.Util.setProp(GoAccess.AppPrefs, panel + '.columns', columns);
		GoAccess.setPrefs();

		GoAccess.Tables.renderThead(panel, GoAccess.getPanelUI(panel));
		GoAccess.Tables.renderFullTable(panel);
	},

	sortColumn: function (ele) {
		var field = ele.getAttribute('data-key');
		var order = ele.getAttribute('data-order');
		var panel = ele.parentElement.parentElement.parentElement.getAttribute('data-panel');

		order = order ? 'asc' == order ? 'desc' : 'asc' : 'asc';
		GoAccess.App.sortData(panel, field, order);
		GoAccess.Util.setProp(GoAccess.AppState, panel + '.sort', {
			'field': field,
			'order': order,
		});
		this.renderThead(panel, GoAccess.getPanelUI(panel));
		this.renderTable(panel, this.getCurPage(panel));

		GoAccess.Charts.reloadChart(GoAccess.AppCharts[panel], panel);
	},

	getDataByKey: function (panel, key) {
		var data = GoAccess.getPanelData(panel).data;
		for (var i = 0, n = data.length; i < n; ++i) {
			if (GoAccess.Util.hashCode(data[i].data) == key)
				return data[i];
		}
		return null;
	},

	getSubItemsData: function (panel) {
		var out = [], items = this.chartData[panel];
		for (var x in items) {
			if (!items.hasOwnProperty(x))
				continue;
			out = out.concat(items[x]);
		}
		return out;
	},

	addChartData: function (panel, key) {
		var data = this.getDataByKey(panel, key);
		var path = panel + '.' + key;

		if (!data || !data.items)
			return [];
		GoAccess.Util.setProp(this.chartData, path, data.items);

		return this.getSubItemsData(panel);
	},

	removeChartData: function (panel, key) {
		if (GoAccess.Util.getProp(this.chartData, panel + '.' + key))
			delete this.chartData[panel][key];

		if (!this.chartData[panel] || Object.keys(this.chartData[panel]).length == 0)
			return GoAccess.getPanelData(panel).data;

		return this.getSubItemsData(panel);
	},

	isExpanded: function (panel, key) {
		var path = panel + '.expanded.' + key;
		return GoAccess.Util.getProp(GoAccess.AppState, path);
	},

	toggleExpanded: function (panel, key) {
		var path = panel + '.expanded.' + key, ret = true;

		if (this.isExpanded(panel, key)) {
			delete GoAccess.AppState[panel]['expanded'][key];
		} else {
			GoAccess.Util.setProp(GoAccess.AppState, path, true), ret = false;
		}

		return ret;
	},

	// Toggle children rows
	toggleRow: function (ele) {
		var hide = false, data = [];
		var row = ele.parentNode;
		var panel = row.getAttribute('data-panel'), key = row.getAttribute('data-key');
		var plotUI = GoAccess.AppCharts[panel].opts();

		hide = this.toggleExpanded(panel, key);
		this.renderTable(panel, this.getCurPage(panel));
		if (!plotUI.redrawOnExpand)
			return;

		if (!hide) {
			data = GoAccess.Charts.processChartData(this.addChartData(panel, key));
		} else {
			data = GoAccess.Charts.processChartData(this.removeChartData(panel, key));
		}
		GoAccess.Charts.drawPlot(panel, plotUI, data);
	},

	// Get current panel page
	getCurPage: function (panel) {
		return GoAccess.Util.getProp(GoAccess.AppState, panel + '.curPage') || 0;
	},

	// Page offset.
	// e.g., Return Value: 11, curPage: 2
	pageOffSet: function (panel) {
		return ((this.getCurPage(panel) - 1) * GoAccess.getPrefs().perPage);
	},

	// Get total number of pages given the number of items on array
	getTotalPages: function (dataItems) {
		return Math.ceil(dataItems.length / GoAccess.getPrefs().perPage);
	},

	// Get a shallow copy of a portion of the given data array and the
	// current page.
	getPage: function (panel, dataItems, page) {
		var totalPages = this.getTotalPages(dataItems);
		if (page < 1)
			page = 1;
		if (page > totalPages)
			page = totalPages;

		GoAccess.Util.setProp(GoAccess.AppState, panel + '.curPage', page);
		var start = this.pageOffSet(panel);
		var end = start + GoAccess.getPrefs().perPage;

		return dataItems.slice(start, end);
	},

	// Get previous page
	prevPage: function (panel) {
		return this.getCurPage(panel) - 1;
	},

	// Get next page
	nextPage: function (panel) {
		return this.getCurPage(panel) + 1;
	},

	getMetaCell: function (ui, o, key) {
		var val =  o && (key in o) && o[key].value ? o[key].value : null;
		var perc = o &&  (key in o) && o[key].percent ? o[key].percent : null;

		// use metaType if exist else fallback to dataType
		var vtype = ui.metaType || ui.dataType;
		var className = ui.className || '';
		className += !['string'].includes(ui.dataType) ? 'text-right' : '';
		return {
			'className': className,
			'value'    : val ? GoAccess.Util.fmtValue(val, vtype) : null,
			'percent'  : perc,
			'title'    : ui.meta,
			'label'    : ui.metaLabel || null,
		};
	},

	hideColumn: function (panel, col) {
		var columns = (GoAccess.getPrefs(panel) || {}).columns || {};
		return ((col in columns) && columns[col]['hide']);
	},

	showTables: function () {
		return ('showTables' in GoAccess.getPrefs()) ? GoAccess.getPrefs().showTables : true;
	},

	autoHideTables: function () {
		return ('autoHideTables' in GoAccess.getPrefs()) ? GoAccess.getPrefs().autoHideTables : true;
	},

	hasTable: function (ui) {
		ui['table'] = this.showTables();
		ui['autoHideTables'] = this.autoHideTables();
	},

	getMetaRows: function (panel, ui, key) {
		var cells = [], uiItems = ui.items;
		var data = GoAccess.getPanelData(panel).metadata;

		for (var i = 0; i < uiItems.length; ++i) {
			var item = uiItems[i];
			if (this.hideColumn(panel, item.key))
				continue;
			cells.push(this.getMetaCell(item, data[item.key], key));
		}

		return [{
			'hasSubItems': ui.hasSubItems,
			'cells': cells,
			'key' : key.substring(0, 3),
		}];
	},

	renderMetaRow: function (panel, metarows, className) {
		// find the table to set
		var table = $('.table-' + panel + ' tr.' + className);
		if (!table)
			return;

		table.innerHTML = GoAccess.AppTpls.Tables.meta.render({
			row: metarows
		});
	},

	// Iterate over user interface definition properties
	iterUIItems: function (panel, uiItems, dataItems, callback) {
		var out = [];
		for (var i = 0; i < uiItems.length; ++i) {
			var uiItem = uiItems[i];
			if (this.hideColumn(panel, uiItem.key))
				continue;
			// Data for the current user interface property.
			// e.g., dataItem = Object {count: 13949, percent: 5.63}
			var dataItem = dataItems[uiItem.key];
			// Apply the callback and push return data to output array
			if (callback && typeof callback == 'function') {
				var ret = callback.call(this, panel, uiItem, dataItem);
				if (ret) out.push(ret);
			}
		}
		return out;
	},

	// Return an object that can be consumed by the table template given a user
	// interface definition and a cell value object.
	// e.g., value = Object {count: 14351, percent: 5.79}
	getObjectCell: function (panel, ui, value) {
		var className = ui.className || '';
		className += !['string'].includes(ui.dataType) ? 'text-right' : '';
		return {
			'className': className,
			'percent': GoAccess.Util.getPercent(value),
			'value': GoAccess.Util.fmtValue(GoAccess.Util.getCount(value), ui.dataType, null, null, ui.hlregex, ui.hlvalue, ui.hlidx)
		};
	},

	// Given a data item object, set all the row cells and return a
	// table row that the template can consume.
	renderRow: function (panel, callback, ui, dataItem, idx, subItem, parentId, expanded) {
		var shadeParent = ((!subItem && idx % 2 != 0) ? 'shaded' : '');
		var shadeChild = ((parentId % 2 != 0) ? 'shaded' : '');
		return {
			'panel'       : panel,
			'idx'         : !subItem && (String((idx + 1) + this.pageOffSet(panel))),
			'key'         : !subItem ? GoAccess.Util.hashCode(dataItem.data) : '',
			'expanded'    : !subItem && expanded,
			'parentId'    : subItem ? String(parentId) : '',
			'className'   : subItem ? 'child ' + shadeChild : 'parent ' + shadeParent,
			'hasSubItems' : ui.hasSubItems,
			'items'       : dataItem.items ? dataItem.items.length : 0,
			'cells'       : callback.call(this),
		};
	},

	renderRows: function (rows, panel, ui, dataItems, subItem, parentId) {
		subItem = subItem || false;
		// no data rows
		if (dataItems.length == 0 && ui.items.length) {
			rows.push({
				cells: [{
					className: 'text-center',
					colspan: ui.items.length + 1,
					value: 'No data on this panel.'
				}]
			});
		}

		// Iterate over all data items for the given panel and
		// generate a table row per date item.
		var cellcb = null;
		for (var i = 0; i < dataItems.length; ++i) {
			var dataItem = dataItems[i], data = null, expanded = false;
			switch(typeof dataItem) {
			case 'string':
				data = dataItem;
				cellcb = function () {
					return {
						'colspan': ui.items.length,
						'value': data
					};
				};
				break;
			default:
				data = dataItem.data;
				cellcb = this.iterUIItems.bind(this, panel, ui.items, dataItem, this.getObjectCell.bind(this));
			}

			expanded = this.isExpanded(panel, GoAccess.Util.hashCode(data));
			rows.push(this.renderRow(panel, cellcb, ui, dataItem, i, subItem, parentId, expanded));
			if (dataItem.items && dataItem.items.length && expanded) {
				this.renderRows(rows, panel, ui, dataItem.items, true, i, expanded);
			}
		}
	},

	// Entry point to render all data rows into the table
	renderDataRows: function (panel, ui, dataItems, page) {
		// find the table to set
		var table = $('.table-' + panel + ' tbody.tbody-data');
		if (!table)
			return;

		dataItems = this.getPage(panel, dataItems, page);
		var rows = [];
		this.renderRows(rows, panel, ui, dataItems);
		if (rows.length == 0)
			return;

		table.innerHTML = GoAccess.AppTpls.Tables.data.render({
			rows: rows
		});
	},

	togglePagination: function (panel, page, dataItems) {
		GoAccess.Panels.enablePagination(panel);
		// Disable pagination next button if last page is reached
		if (page >= this.getTotalPages(dataItems)) {
			GoAccess.Panels.disableNext(panel);
			GoAccess.Panels.disableLast(panel);
		}
		if (page <= 1) {
			GoAccess.Panels.disablePrev(panel);
			GoAccess.Panels.disableFirst(panel);
		}
	},

	renderTable: function (panel, page) {
		var dataItems = GoAccess.getPanelData(panel).data;
		var ui = GoAccess.getPanelUI(panel);

		if (page === "LAST_PAGE") {
			page = this.getTotalPages(dataItems);
		} else if (page === "FIRST_PAGE") {
			page = 1;
		}

		this.togglePagination(panel, page, dataItems);
		// Render data rows
		this.renderDataRows(panel, ui, dataItems, page);
		this.events();
	},

	renderFullTable: function (panel) {
		var ui = GoAccess.getPanelUI(panel), page = 0;
		// panel's data
		var data = GoAccess.getPanelData(panel);

		// render meta data
		if (data.hasOwnProperty('metadata')) {
			this.renderMetaRow(panel, this.getMetaRows(panel, ui, 'min'), 'thead-min');
			this.renderMetaRow(panel, this.getMetaRows(panel, ui, 'max'), 'thead-max');
			this.renderMetaRow(panel, this.getMetaRows(panel, ui, 'avg'), 'thead-avg');
		}

		// render actual data
		if (data.hasOwnProperty('data')) {
			page = this.getCurPage(panel);
			this.togglePagination(panel, page, data.data);
			this.renderDataRows(panel, ui, data.data, page);
		}

		// render meta data
		if (data.hasOwnProperty('metadata')) {
			this.renderMetaRow(panel, this.getMetaRows(panel, ui, 'total'), 'tfoot-totals');
		}
	},

	// Iterate over all panels and determine which ones should contain
	// a data table.
	renderTables: function (force) {
		var ui = GoAccess.getPanelUI();
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel) || GoAccess.Util.isPanelHidden(panel) || !this.showTables())
				continue;
			if (force || GoAccess.Util.isWithinViewPort($('#panel-' + panel)))
				this.renderFullTable(panel);
		}
	},

	// Given a UI panel definition, make a copy of it and assign the sort
	// fields to the template object to render
	sort2Tpl: function (panel, ui) {
		var uiClone = JSON.parse(JSON.stringify(ui)), out = [];
		var sort = GoAccess.Util.getProp(GoAccess.AppState, panel + '.sort');

		for (var i = 0, len = uiClone.items.length; i < len; ++i) {
			var item = uiClone.items[i];
			if (this.hideColumn(panel, item.key))
				continue;

			item['sort'] = false;
			if (item.key == sort.field && sort.order) {
				item['sort'] = true;
				item[sort.order.toLowerCase()] = true;
			}
			out.push(item);
		}
		uiClone.items = out;

		return uiClone;
	},

	renderThead: function (panel, ui) {
		var $thead = $('.table-' + panel + '>thead>tr.thead-cols'),
			$colgroup = $('.table-' + panel + '>colgroup');

		if ($thead && $colgroup && this.showTables()) {
			ui = this.sort2Tpl(panel, ui);

			$thead.innerHTML = GoAccess.AppTpls.Tables.head.render(ui);
			$colgroup.innerHTML = GoAccess.AppTpls.Tables.colgroup.render(ui);
		}
	},

	reloadTables: function () {
		this.renderTables(false);
		this.events();
	},

	initialize: function () {
		this.renderTables(true);
		this.events();

		// redraw on scroll
		d3.select(window).on('scroll.tables', debounce(function () {
			this.reloadTables();
		}, 250, false).bind(this));
	},
};

// Main App
GoAccess.App = {
	hasFocus: true,

	tpl: function (tpl) {
		return Hogan.compile(tpl);
	},

	setTpls: function () {
		GoAccess.AppTpls = {
			'Nav': {
				'wrap': this.tpl($('#tpl-nav-wrap').innerHTML),
				'menu': this.tpl($('#tpl-nav-menu').innerHTML),
				'opts': this.tpl($('#tpl-nav-opts').innerHTML),
			},
			'Panels': {
				'wrap': this.tpl($('#tpl-panel').innerHTML),
				'opts': this.tpl($('#tpl-panel-opts').innerHTML),
			},
			'General': {
				'wrap': this.tpl($('#tpl-general').innerHTML),
				'items': this.tpl($('#tpl-general-items').innerHTML),
			},
			'Tables': {
				'colgroup': this.tpl($('#tpl-table-colgroup').innerHTML),
				'head': this.tpl($('#tpl-table-thead').innerHTML),
				'meta': this.tpl($('#tpl-table-row-meta').innerHTML),
				'totals': this.tpl($('#tpl-table-row-totals').innerHTML),
				'data': this.tpl($('#tpl-table-row').innerHTML),
			},
		};
	},

	sortField: function (o, field) {
	   var f = o[field];
	   if (GoAccess.Util.isObject(f) && (f !== null))
		   f = o[field].count;
		return f;
	},

	sortData: function (panel, field, order) {
		// panel's data
		var panelData = GoAccess.getPanelData(panel).data;

		// Function to sort an array of objects
		var sortArray = function(arr) {
			arr.sort(function (a, b) {
				a = this.sortField(a, field);
				b = this.sortField(b, field);

				if (typeof a === 'string' && typeof b === 'string')
					return 'asc' == order ? a.localeCompare(b) : b.localeCompare(a);
				return  'asc' == order ? a - b : b - a;
			}.bind(this));
		}.bind(this);

		// Sort panelData
		sortArray(panelData);

		// Sort the items sub-array
		panelData.forEach(function(item) {
			if (item.items) {
				sortArray(item.items);
			}
		});
	},

	setInitSort: function () {
		var ui = GoAccess.getPanelUI();
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel))
				continue;
			GoAccess.Util.setProp(GoAccess.AppState, panel + '.sort', ui[panel].sort);
		}
	},

	// Verify if we need to sort panels upon data re-entry
	verifySort: function () {
		var ui = GoAccess.getPanelUI();
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel) || GoAccess.Util.isPanelHidden(panel))
				continue;
			var sort = GoAccess.Util.getProp(GoAccess.AppState, panel + '.sort');
			// do not sort panels if they still hold the same sort properties
			if (JSON.stringify(sort) === JSON.stringify(ui[panel].sort))
				continue;
			this.sortData(panel, sort.field, sort.order);
		}
	},

	initDom: function () {
		$('nav').classList.remove('hide');
		$('.container').classList.remove('hide');
		$('.spinner').classList.add('hide');
		$('.app-loading-status > small').style.display = 'none';

		if (GoAccess.AppPrefs['layout'] == 'horizontal' || GoAccess.AppPrefs['layout'] == 'wide') {
			$('.container').classList.add('container-fluid');
			$('.container-fluid').classList.remove('container');
		}
	},

	renderData: function () {
		// update data and charts if tab/document has focus
		if (!this.hasFocus)
			return;

		// some panels may not have been properly rendered since no data was
		// passed when bootstrapping the report, thus we do a one full
		// re-render of all panels
		if (GoAccess.OverallStats.total_requests == 0 && GoAccess.OverallStats.total_requests != GoAccess.AppData.general.total_requests)
			GoAccess.Panels.initialize();
		GoAccess.OverallStats.total_requests = GoAccess.AppData.general.total_requests;

		this.verifySort();
		GoAccess.OverallStats.initialize();

		// do not rerender tables/charts if data hasn't changed
		if (!GoAccess.AppState.updated)
			return;

		GoAccess.Charts.reloadCharts();
		GoAccess.Tables.reloadTables();
	},

	renderPanels: function () {
		GoAccess.Nav.initialize();
		GoAccess.OverallStats.initialize();
		GoAccess.Panels.initialize();
		GoAccess.Charts.initialize();
		GoAccess.Tables.initialize();
	},

	initialize: function () {
		this.setInitSort();
		this.setTpls();
		this.initDom();
		this.renderPanels();
	},
};

// Adds the visibilitychange EventListener
document.addEventListener('visibilitychange', function () {
	// fires when user switches tabs, apps, etc.
	if (document.visibilityState === 'hidden')
		GoAccess.App.hasFocus = false;

	// fires when app transitions from hidden or user returns to the app/tab.
	if (document.visibilityState === 'visible' && GoAccess.isAppInitialized) {
		var hasFocus = GoAccess.App.hasFocus;
		GoAccess.App.hasFocus = true;
		hasFocus || GoAccess.App.renderData();
	}
});

// Init app
window.onload = function () {
	GoAccess.initialize({
		'i18n': window.json_i18n,
		'uiData': window.user_interface,
		'panelData': window.json_data,
		'wsConnection': window.connection || null,
		'prefs': window.html_prefs || {},
	});
};
}());
