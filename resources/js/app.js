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

// global namespace
window.GoAccess = window.GoAccess || {
	initialize: function (options) {
		this.opts = options;

		this.AppState  = {}; // current state app key-value store
		this.AppTpls   = {}; // precompiled templates
		this.AppCharts = {}; // holds all rendered charts
		this.AppUIData = (this.opts || {}).uiData || {};    // holds panel definitions
		this.AppData   = (this.opts || {}).panelData || {}; // hold raw data
		this.AppWSConn = (this.opts || {}).wsConnection || {}; // WebSocket connection
		this.AppPrefs  = {
			'theme': 'darkGray',
			'perPage': 10,
		};

		var ls = JSON.parse(localStorage.getItem('AppPrefs'));
		this.AppPrefs = GoAccess.Util.merge(this.AppPrefs, ls);
		if (Object.keys(this.AppWSConn).length)
			this.setWebSocket(this.AppWSConn);
	},

	getPanelUI: function (panel) {
		return panel ? this.AppUIData[panel] : this.AppUIData;
	},

	getPrefs: function () {
		return this.AppPrefs;
	},

	setPrefs: function () {
		localStorage.setItem('AppPrefs', JSON.stringify(GoAccess.getPrefs()));
	},

	getPanelData: function (panel) {
		return panel ? this.AppData[panel] : this.AppData;
	},

	setWebSocket: function (wsConn) {
		var str = wsConn.url.indexOf(':') != -1 ? wsConn.url : String(wsConn.url + ':' + wsConn.port);
		var socket = new WebSocket('ws://' + str);
		socket.onopen = function (event) {
			GoAccess.Nav.WSOpen();
		}.bind(this);
		socket.onmessage = function (event) {
			this.AppData = JSON.parse(event.data);
			this.App.renderData();
		}.bind(this);
		socket.onclose = function (event) {
			GoAccess.Nav.WSClose();
		}.bind(this);
	},
};

// HELPERS
GoAccess.Util = {
	months: ["Jan", "Feb", "Ma", "Apr", "May", "Jun", "Jul","Aug", "Sep", "Oct", "Nov", "Dec"],

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

	// Format bytes to human readable
	formatBytes: function (bytes, decimals, numOnly) {
		if (bytes == 0)
			return numOnly ? 0 : '0 Byte';
		var k = 1024;
		var dm = decimals + 1 || 3;
		var sizes = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
		var i = Math.floor(Math.log(bytes) / Math.log(k));
		return (bytes / Math.pow(k, i)).toPrecision(dm) + (numOnly ? '' : (' ' + sizes[i]));
	},

	// Validate number
	isNumeric: function (n) {
		return !isNaN(parseFloat(n)) && isFinite(n);
	},

	// Format microseconds to human readable
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

	// Format field value to human readable
	fmtValue: function (value, valueType) {
		var val = 0;
		if (!valueType)
			val = value;

		switch (valueType) {
		case 'utime':
			val = this.utime2str(value);
			break;
		case 'date':
			val = this.formatDate(value);
			break;
		case 'numeric':
			if (this.isNumeric(value))
				val = value.toLocaleString();
			break;
		case 'bytes':
			val = this.formatBytes(value);
			break;
		case 'percent':
			val = value.toFixed(2) + '%';
			break;
		case 'time':
			if (this.isNumeric(value))
				val = value.toLocaleString();
			break;
		default:
			val = value;
		};

		return value == 0 ? String(val) : val;
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
			if (!schema[k]) schema[k] = {}
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
};

// OVERALL STATS
GoAccess.OverallStats = {
	// Render general section wrapper
	renderWrapper: function (ui) {
		$('.wrap-general').innerHTML = GoAccess.AppTpls.General.wrap.render(ui);
	},

	// Render each overall stats box
	renderBox: function (data, ui, row, x, idx) {
		var wrap = $('.wrap-general-items');

		// create a new bootstrap row every 6 elements
		if (idx % 6 == 0) {
			row = document.createElement('div');
			row.setAttribute('class', 'row');
			wrap.appendChild(row);
		}

		var box = document.createElement('div');
		box.innerHTML = GoAccess.AppTpls.General.items.render({
			'className': ui.items[x].className,
			'label': ui.items[x].label,
			'value': GoAccess.Util.fmtValue(data[x], ui.items[x].valueType),
		});
		row.appendChild(box);

		return row;
	},

	// Render overall stats
	renderData: function (data, ui) {
		var idx = 0, row = null;

		$('.last-updated').innerHTML = data.date_time;
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
		var data = GoAccess.getPanelData('general'), i = 0;

		this.renderWrapper(ui);
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

		$$('[data-perpage]', function (item) {
			item.onclick = function (e) {
				this.setPerPage(e);
			}.bind(this);
		}.bind(this));
	},

	downloadJSON: function (e) {
		var targ = e.currentTarget;
		var data = "text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(GoAccess.getPanelData()));
		targ.href = 'data:' + data;
		targ.download = 'goaccess-' + +new Date() + '.json';
	},

	setTheme: function (theme) {
		if (!theme)
			return;

		$('html').className = '';
		switch(theme) {
		case 'darkGray':
			$('html').classList.add('dark');
			$('html').classList.add('gray');
			break;
		case 'darkBlue':
			$('html').classList.add('dark');
			$('html').classList.add('blue');
			break;
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
		case 'geolocation'     : return 'location-arrow';
		case 'status_codes'    : return 'warning';
		default                : return 'pie-chart';
		}
	},

	getItems: function () {
		var ui = GoAccess.getPanelUI(), menu = [];
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel))
				continue;
			// Push valid panels to our navigation array
			menu.push({
				'current': window.location.hash.substr(1) == panel,
				'head': ui[panel].head,
				'key': panel,
				'icon': this.getIcon(panel),
			});
		}
		return menu;
	},

	setPerPage: function (e) {
		GoAccess.AppPrefs['perPage'] = +e.currentTarget.getAttribute('data-perpage');
		GoAccess.App.renderData();
		GoAccess.setPrefs();
	},

	getTheme: function () {
		return GoAccess.AppPrefs.theme || 'darkGray';
	},

	getPerPage: function () {
		return GoAccess.AppPrefs.perPage || 10;
	},

	// Render left-hand side navigation options.
	renderOpts: function () {
		var o = {};
		o[this.getTheme()] = true;
		o['perPage' + this.getPerPage()] = true;

		$('.nav-list').innerHTML = GoAccess.AppTpls.Nav.opts.render(o);
		$('nav').classList.toggle('active');
		this.events();
	},

	// Render left-hand side navigation given the available panels.
	renderMenu: function (e) {
		$('.nav-list').innerHTML = GoAccess.AppTpls.Nav.menu.render({
			'nav': this.getItems(),
			'overall': window.location.hash.substr(1) == '',
		});
		$('nav').classList.toggle('active');
		this.events();
	},

	WSStatus: function () {
		if (Object.keys(GoAccess.AppWSConn).length)
			$$('.nav-ws-status', function (item) { item.style.display = 'block' });
	},

	WSClose: function () {
		$$('.nav-ws-status', function (item) {
			item.classList.remove('connected');
			item.setAttribute('title', 'Disconnected');
		});
	},

	WSOpen: function () {
		$$('.nav-ws-status', function (item) {
			item.classList.add('connected');
			item.setAttribute('title', 'Connected to ' + GoAccess.AppWSConn.url);
		});
	},

	// Render left-hand side navigation given the available panels.
	renderWrap: function (nav) {
		$('nav').innerHTML = GoAccess.AppTpls.Nav.wrap.render({});
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
	enablePrev: function (panel) {
		var pagination = '#panel-' + panel + ' .pagination a';
		$(pagination + '.panel-prev').parentNode.classList.remove('disabled');
	},

	disablePrev: function (panel) {
		var pagination = '#panel-' + panel + ' .pagination a';
		$(pagination + '.panel-prev').parentNode.classList.add('disabled');
	},

	enableNext: function (panel) {
		var pagination = '#panel-' + panel + ' .pagination a';
		$(pagination + '.panel-next').parentNode.classList.remove('disabled');
	},

	disableNext: function (panel) {
		var pagination = '#panel-' + panel + ' .pagination a';
		$(pagination + '.panel-next').parentNode.classList.add('disabled');
	},

	enablePagination: function (panel) {
		this.enablePrev(panel);
		this.enableNext(panel);
	},

	disablePagination: function (panel) {
		this.disablePrev(panel);
		this.disableNext(panel);
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

	// Render the given panel given a user interface definition.
	renderPanel: function (panel, ui) {
		var data = GoAccess.getPanelData(panel);
		this.hasSubItems(ui, data.data);

		var box = document.createElement('div');
		box.id = 'panel-' + panel;
		box.innerHTML = GoAccess.AppTpls.Panels.wrap.render(ui);
		$('.wrap-panels').appendChild(box);

		// Remove pagination if not enough data for the given panel
		if (data.data.length <= GoAccess.getPrefs().perPage)
			this.disablePagination(panel);
		GoAccess.Tables.renderThead(panel, ui);
	},

	// Iterate over all available panels and render each panel
	// structure.
	renderPanels: function () {
		var ui = GoAccess.getPanelUI();
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel))
				continue;
			// Render panel given a user interface definition
			this.renderPanel(panel, ui[panel]);
		}
	},

	initialize: function () {
		this.renderPanels();
	}
};

// RENDER CHARTS
GoAccess.Charts = {
	events: function () {
		$$('[data-plot]', function (item) {
			item.onclick = function (e) {
				this.redrawChart(e.currentTarget);
			}.bind(this);
		}.bind(this));

		$$('[data-chart-type]', function (item) {
			item.onclick = function (e) {
				this.setChartType(e.currentTarget);
			}.bind(this);
		}.bind(this));
	},

	setChartType: function (targ) {
		var panel = targ.getAttribute('data-panel');
		var type = targ.getAttribute('data-chart-type');

		GoAccess.Util.setProp(GoAccess.AppPrefs, panel + '.plot', {'chartType': type});
		GoAccess.setPrefs();

		var plotUI = GoAccess.Util.getProp(GoAccess.AppState, panel + '.plot');
		// Extract data for the selected panel and process it
		this.drawPlot(panel, plotUI, this.getPanelData(panel));
	},

	getPanelData: function (panel, data) {
		// Grab ui plot data for the selected panel
		var plot = GoAccess.Util.getProp(GoAccess.AppState, panel + '.plot');

		// Grab the data for the selected panel
		data = data || this.processChartData(GoAccess.getPanelData(panel).data);
		return plot.chartReverse ? data.reverse() : data;
	},

	renderChart: function (panel, chart, data) {
		// remove popup
		d3.select('#chart-' + panel + '>.chart-tooltip-wrap')
			.remove();
		// remove svg
		d3.select('#chart-' + panel).select('svg')
			.remove();
		// add chart to the document
		d3.select("#chart-" + panel)
			.datum(data)
			.call(chart)
			.append("div").attr("class", "chart-tooltip-wrap");
	},

	drawPlot: function (panel, plotUI, data) {
		var chart = this.getChart(panel, plotUI, data);
		if (!chart)
			return;

		this.renderChart(panel, chart, data);
		GoAccess.AppCharts[panel] = null;
		GoAccess.AppCharts[panel] = chart;
	},

	// Redraw a chart upon selecting a metric.
	redrawChart: function (targ) {
		var plot = targ.getAttribute('data-plot');
		var panel = targ.getAttribute('data-panel');
		var ui = GoAccess.getPanelUI(panel);
		var plotUI = ui.plot;

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

	// Iterate over the item properties and and extract the count value.
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
		var items = GoAccess.getPanelUI(panel).items, o = {};
		for (var i = 0; i < items.length; ++i) {
			if (items[i].key == key)
				return items[i];
		}
		return null;
	},

	getAreaSpline: function (panel, plotUI, data) {
		var dualYaxis = plotUI['d3']['y1'];

		var chart = AreaChart(dualYaxis)
		.labels({
			y0: plotUI['d3']['y0'].label,
			y1: dualYaxis ? plotUI['d3']['y1'].label : ''
		})
		.x(function (d) {
			return d.data;;
		})
		.y0(function (d) {
			return +d[plotUI['d3']['y0']['key']];
		})
		.width($("#chart-" + panel).offsetWidth)
		.height(175)
		.format({
			x: (this.findUIItem(panel, 'data') || {}).valueType || null,
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
			return d.data;;
		})
		.y0(function (d) {
			return +d[plotUI['d3']['y0']['key']];
		})
		.width($("#chart-" + panel).offsetWidth)
		.height(175)
		.format({
			x: (this.findUIItem(panel, 'data') || {}).valueType || null,
			y0: ((plotUI.d3 || {}).y0 || {}).format,
			y1: ((plotUI.d3 || {}).y1 || {}).format,
		})
		.opts(plotUI);

		dualYaxis && chart.y1(function (d) {
			return +d[plotUI['d3']['y1']['key']];
		});

		return chart;
	},

	getChart: function (panel, plotUI, data) {
		var chart = null, type = null;
		type = GoAccess.Util.getProp(GoAccess.getPrefs(), panel + '.plot.chartType') || plotUI.chartType;

		// Render given its type
		switch (type) {
		case 'area-spline':
			chart = this.getAreaSpline(panel, plotUI, data);
			break;
		case 'bar':
			chart = this.getVBar(panel, plotUI, data);
			break;
		};

		return chart;
	},

	// Render all charts for the applicable panels.
	renderCharts: function (ui) {
		var plotUI = null, chart = null;
		for (var panel in ui) {
			if (!ui.hasOwnProperty(panel))
				continue;
			// Ensure it has a plot definitions
			if (!ui[panel].plot || !ui[panel].plot.length)
				continue;

			plotUI = ui[panel].plot[0];
			// set ui plot data
			GoAccess.Util.setProp(GoAccess.AppState, panel + '.plot', plotUI);

			// Grab the data for the selected panel
			var data = this.getPanelData(panel);
			if (!(chart = this.getChart(panel, plotUI, data)))
				continue;

			this.renderChart(panel, chart, data);
			GoAccess.AppCharts[panel] = chart;
		}
	},

	reloadChart: function () {
		var ui = GoAccess.getPanelUI();
		Object.keys(GoAccess.AppCharts).forEach(function (panel) {
			var subItems = GoAccess.Tables.getSubItemsData(panel);
			var data = (subItems.length ? subItems : GoAccess.getPanelData(panel).data).slice(0);

			d3.select("#chart-" + panel)
				.datum(this.processChartData(this.getPanelData(panel, data)))
				.call(GoAccess.AppCharts[panel].width($("#chart-" + panel).offsetWidth));
		}.bind(this));
	},

	initialize: function () {
		this.renderCharts(GoAccess.getPanelUI());

		// Resize charts
		d3.select(window).on('resize', function () {
			Object.keys(GoAccess.AppCharts).forEach(function (panel) {
				d3.select("#chart-" + panel)
					.call(GoAccess.AppCharts[panel].width($("#chart-" + panel).offsetWidth));
			});
		});
		this.events();
	}
};

// RENDER TABLES
GoAccess.Tables = {
	chartData: {}, // holds all panel sub items data that feeds the chart

	events: function () {
		$$('.panel-next', function (item) {
			item.onclick = function (e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				this.renderTable(panel, this.nextPage(panel))
			}.bind(this);
		}.bind(this));

		$$('.panel-prev', function (item) {
			item.onclick = function (e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				this.renderTable(panel, this.prevPage(panel))
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

	sortColumn: function (ele) {
		var field = ele.getAttribute('data-key');
		var order = ele.getAttribute('data-order');
		var panel = ele.parentElement.parentElement.parentElement.getAttribute('data-panel');

		order = order ? 'asc' == order ? 'desc' : 'asc' : 'asc';
		GoAccess.App.sortData(panel, field, order)
		GoAccess.Util.setProp(GoAccess.AppState, panel + '.sort', {
			'field': field,
			'order': order,
		});
		this.renderThead(panel, GoAccess.getPanelUI(panel));
		this.renderTable(panel, this.getCurPage(panel));
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

		if (this.isExpanded(panel, key))
			delete GoAccess.AppState[panel]['expanded'][key];
		else
			GoAccess.Util.setProp(GoAccess.AppState, path, true), ret = false;

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

		if (!hide)
			data = GoAccess.Charts.processChartData(this.addChartData(panel, key));
		else
			data = GoAccess.Charts.processChartData(this.removeChartData(panel, key));
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

	getMetaValue: function (ui, value) {
		if ('meta' in ui)
			return value[ui.meta];
		return null;
	},

	getMetaCell: function (ui, value) {
		var val = this.getMetaValue(ui, value);
		var max = (value || {}).max;
		var min = (value || {}).min;

		var className = ui.className || '';
		className += ui.valueType != 'string' ? 'text-right' : '';
		return {
			'className': className,
			'max'      : max != undefined ? GoAccess.Util.fmtValue(max, ui.valueType) : null,
			'min'      : min != undefined ? GoAccess.Util.fmtValue(min, ui.valueType) : null,
			'value'    : val != undefined ? GoAccess.Util.fmtValue(val, ui.valueType) : null,
			'title'    : ui.meta,
		}
	},

	renderMetaRow: function (panel, ui) {
		// find the table to set
		var table = $('.table-' + panel + ' tbody.tbody-meta');
		if (!table)
			return;

		var cells = [], uiItems = ui.items;
		var data = GoAccess.getPanelData(panel).metadata;
		for (var i = 0; i < uiItems.length; ++i) {
			var item = uiItems[i];
			var value = data[item.key];
			cells.push(this.getMetaCell(item, value))
		}

		table.innerHTML = GoAccess.AppTpls.Tables.meta.render({
			row: [{
				'hasSubItems': ui.hasSubItems,
				'cells': cells
			}]
		});
	},

	// Iterate over user interface definition properties
	iterUIItems: function (panel, uiItems, dataItems, callback) {
		var out = [];
		for (var i = 0; i < uiItems.length; ++i) {
			var uiItem = uiItems[i];
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
		className += ui.valueType != 'string' ? 'text-right' : '';
		return {
			'className': className,
			'percent': GoAccess.Util.getPercent(value),
			'value': GoAccess.Util.fmtValue(GoAccess.Util.getCount(value), ui.valueType)
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
					}
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

		var dataItems = this.getPage(panel, dataItems, page);
		var rows = [];
		this.renderRows(rows, panel, ui, dataItems);
		if (rows.length == 0)
			return;

		table.innerHTML = GoAccess.AppTpls.Tables.data.render({
			rows: rows
		});
	},

	renderTable: function (panel, page, expanded) {
		var dataItems = GoAccess.getPanelData(panel).data;
		var ui = GoAccess.getPanelUI(panel);

		GoAccess.Panels.enablePagination(panel);
		// Diable pagination next button if last page is reached
		if (page >= this.getTotalPages(dataItems))
			GoAccess.Panels.disableNext(panel);
		if (page <= 1)
			GoAccess.Panels.disablePrev(panel);

		// Render data rows
		this.renderDataRows(panel, ui, dataItems, page);
		this.events();
	},

	// Iterate over all panels and determine which ones should contain
	// a data table.
	renderTables: function () {
		var ui = GoAccess.getPanelUI();
		for (var panel in ui) {
			if (GoAccess.Util.isPanelValid(panel))
				continue;

			// panel's data
			var data = GoAccess.getPanelData(panel);
			// render meta data
			if (data.hasOwnProperty('metadata'))
				this.renderMetaRow(panel, ui[panel]);
			// render actual data
			if (data.hasOwnProperty('data')) {
				this.renderDataRows(panel, ui[panel], data.data, this.getCurPage(panel));
			}
		}
	},

	// Given a UI panel definition, make a copy of it and assign the sort
	// fields to the template object to render
	sort2Tpl: function (panel, ui) {
		var uiClone = JSON.parse(JSON.stringify(ui));;
		var sort = GoAccess.Util.getProp(GoAccess.AppState, panel + '.sort');
		uiClone['items'].forEach(function (item) {
			item['sort'] = false;
			if (item.key == sort.field && sort.order) {
				item['sort'] = true;
				item[sort.order.toLowerCase()] = true;
			}
		});

		return uiClone;
	},

	renderThead: function (panel, ui) {
		$('.table-' + panel + '>thead').innerHTML = GoAccess.AppTpls.Tables.head.render(this.sort2Tpl(panel, ui));
	},

	initialize: function () {
		this.renderTables();
		this.events();
	},
};

// Main App
GoAccess.App = {
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
			},
			'General': {
				'wrap': this.tpl($('#tpl-general').innerHTML),
				'items': this.tpl($('#tpl-general-items').innerHTML),
			},
			'Tables': {
				'head': this.tpl($('#tpl-table-thead').innerHTML),
				'meta': this.tpl($('#tpl-table-row-meta').innerHTML),
				'data': this.tpl($('#tpl-table-row').innerHTML),
			},
		}
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
		panelData.sort(function (a, b) {
			var a = this.sortField(a, field);
			var b = this.sortField(b, field);

			if (typeof a === 'string' && typeof b === 'string')
				return 'asc' == order ? a.localeCompare(b) : b.localeCompare(a);
			return  'asc' == order ? a - b : b - a;
		}.bind(this));
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
			if (GoAccess.Util.isPanelValid(panel))
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
	},

	renderData: function () {
		this.verifySort();
		GoAccess.OverallStats.initialize();
		GoAccess.Charts.reloadChart();
		GoAccess.Tables.initialize();
	},

	initialize: function () {
		this.setInitSort();
		this.setTpls();
		GoAccess.Nav.initialize();
		this.initDom();
		GoAccess.OverallStats.initialize();
		GoAccess.Panels.initialize();
		GoAccess.Charts.initialize();
		GoAccess.Tables.initialize();
	},
};

// Init app
window.onload = function () {
	GoAccess.initialize({
		'uiData': window.user_interface,
		'panelData': window.json_data,
		'wsConnection': window.connection || null,
	});
	GoAccess.App.initialize();

	/*!
	 * Bootstrap without jQuery v0.6.1 for Bootstrap 3
	 * By Daniel Davis under MIT License
	 * https://github.com/tagawa/bootstrap-without-jquery
	 */
	function doDropdown(event) {
		event = event || window.event;
		var evTarget = event.currentTarget || event.srcElement;
		evTarget.parentElement.classList.toggle('open');
		return false;
	}

	function closeDropdown(event) {
		event = event || window.event;
		var evTarget = event.currentTarget || event.srcElement;

		// hacky but FF needs it
		window.setTimeout(function () {
			evTarget.parentElement.classList.remove('open');
		}, 150);

		// Trigger the click event on the target if not opening another menu
		if (event.relatedTarget && event.relatedTarget.getAttribute('data-toggle') !== 'dropdown') {
			event.relatedTarget.click();
		}
		return false;
	}
	var dropdownList = document.querySelectorAll('[data-toggle=dropdown]');
	for (var k = 0, dropdown, lenk = dropdownList.length; k < lenk; k++) {
		dropdown = dropdownList[k];
		dropdown.setAttribute('tabindex', '0'); // Fix to make onblur work in Chrome
		dropdown.onclick = doDropdown;
		dropdown.onblur = closeDropdown;
	}
};
