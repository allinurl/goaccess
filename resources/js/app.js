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

		this.AppCharts = {}; // holds all rendered charts
		this.AppUIData = (this.opts || {}).uiData || {};    // holds panel definitions
		this.AppData   = (this.opts || {}).panelData || {}; // hold raw data
		this.AppPrefs = {
			'perPage': 11, // panel rows per page
		};
	},

	getPrefs: function () {
		return this.AppPrefs;
	},

	getPanelData: function (panel) {
		return panel ? this.AppData[panel] : this.AppData;
	},

	getPanelUI: function (panel) {
		return panel ? this.AppUIData[panel] : this.AppUIData;
	}
};

// HELPERS
GoAccess.Common = {
	months: ["Jan", "Feb", "Ma", "Apr", "May", "Jun", "Jul","Aug", "Sep", "Oct", "Nov", "Dec"],

	// Add all attributes of n to o
	merge: function (o, n) {
		for (var attrname in n) {
			o[attrname] = n[attrname];
		}
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
	formatDate: function (value) {
		var d = parseInt(value);
		d = new Date(d / 1E4, (d % 1E4 / 100) - 1, d % 100);
		return ('0' + d.getDate()).slice(-2) + '/' + this.months[d.getMonth()] + '/' + d.getFullYear();
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

	// Attempts to extract the count from either an object or the actual value.
	// e.g., item = Object {count: 14351, percent: 5.79} OR item = 4824825140
	getCount: function (item) {
		if (typeof item == 'object' && 'count' in item)
			return item.count;
		return item;
	},

	getPercent: function (item) {
		if (typeof item == 'object' && 'percent' in item)
			return this.fmtValue(item.percent, 'percent');
		return null;
	},
};

// OVERALL STATS
GoAccess.OverallStats = {
	template: $('#tpl-general-items').innerHTML,

	// Render general section wrapper
	renderWrapper: function (ui) {
		var template = $('#tpl-general').innerHTML;
		$('.wrap-general').innerHTML = Hogan.compile(template).render(ui);
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
		box.innerHTML = Hogan.compile(this.template).render({
			'className': ui.items[x].className,
			'label': ui.items[x].label,
			'value': GoAccess.Common.fmtValue(data[x], ui.items[x].valueType),
		});
		row.appendChild(box);

		return row;
	},

	// Render overall stats
	renderData: function (data, ui) {
		var idx = 0, row = null;

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
	// Render left-hand side navigation given the available panels.
	renderNav: function (nav) {
		var template = $('#tpl-panel-nav').innerHTML;
		$('.panel-nav').innerHTML = Hogan.compile(template).render({
			nav: nav
		});
	},

	// Iterate over all available panels and render each.
	initialize: function () {
		var ui = GoAccess.getPanelUI();

		var nav = [];
		for (var panel in ui) {
			if (GoAccess.Common.isPanelValid(panel))
				continue;
			// Push valid panels to our navigation array
			nav.push({
				'key': panel,
				'head': ui[panel].head,
			});
		}
		this.renderNav(nav);
	}
};

// RENDER PANELS
GoAccess.Panels = {
	template: $('#tpl-panel').innerHTML,

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
		box.innerHTML = Hogan.compile(this.template).render(ui);
		$('.wrap-panels').appendChild(box);

		// Remove pagination if not enough data for the given panel
		if (data.data.length <= GoAccess.getPrefs()['perPage']) {
			this.disablePagination(panel);
		}
	},

	// Iterate over all available panels and render each panel
	// structure.
	renderPanels: function () {
		var ui = GoAccess.getPanelUI();
		for (var panel in ui) {
			if (GoAccess.Common.isPanelValid(panel))
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
		var _this = this;
		$$('[data-plot]', function (item) {
			item.onclick = function (e) {
				var targ = e.currentTarget;
				_this.redrawChart(targ);
			};
		});
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

			// Extract data for the selected panel and process it
			var data = this.processChartData(GoAccess.getPanelData(panel).data);
			if (ui.chartReverse)
				data = data.reverse();

			d3.select('#chart-' + panel).select('svg').remove();
			this.renderAreaSpline(panel, plotUI[x], data);
			break;
		}
	},

	// Iterate over the item properties and and extract the count value.
	extractCount: function (item) {
		var o = {};
		for (var prop in item)
			o[prop] = GoAccess.Common.getCount(item[prop]);
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

	// Set default D3 chart to area spline and apply panel user
	// interface definition and load data.
	renderAreaSpline: function (panel, plotData, data) {
		var dualYaxis = plotData['d3']['y1'];
		var chart = AreaChart(dualYaxis)
			.labels({
				y0: plotData['d3']['y0'].label,
				y1: dualYaxis ? plotData['d3']['y1'].label : ''
			})
			.x(function (d) {
				return d.data;;
			})
			.y0(function (d) {
				return +d[plotData['d3']['y0']['key']];
			})
			.width($("#chart-" + panel).offsetWidth)
			.height(175)
			.format({
				x: ((plotData.d3 || {}).x || {}).format,
				y0: ((plotData.d3 || {}).y0 || {}).format,
				y1: ((plotData.d3 || {}).y1 || {}).format,
			});

		dualYaxis && chart.y1(function (d) {
			return +d[plotData['d3']['y1']['key']];
		});

		d3.select("#chart-" + panel)
			.datum(data)
			.call(chart)
			.append("div").attr("class", "chart-tooltip-wrap");

		GoAccess.AppCharts[panel] = chart;
	},

	// Set default C3 chart to area spline and apply panel user
	// interface definition and load data.
	renderVBar: function (panel, plotData, data) {
		var dualYaxis = plotData['d3']['y1'];
		var chart = BarChart(dualYaxis)
			.labels({
				y0: plotData['d3']['y0'].label,
				y1: dualYaxis ? plotData['d3']['y1'].label : ''
			})
			.x(function (d) {
				return d.data;;
			})
			.y0(function (d) {
				return +d[plotData['d3']['y0']['key']];
			})
			.width($("#chart-" + panel).offsetWidth)
			.height(175)
			.format({
				x: ((plotData.d3 || {}).x || {}).format,
				y0: ((plotData.d3 || {}).y0 || {}).format,
				y1: ((plotData.d3 || {}).y1 || {}).format,
			});

		dualYaxis && chart.y1(function (d) {
			return +d[plotData['d3']['y1']['key']];
		});

		d3.select("#chart-" + panel)
			.datum(data)
			.call(chart)
			.append("div").attr("class", "chart-tooltip-wrap");

		GoAccess.AppCharts[panel] = chart;
	},

	// Render all charts for the applicable panels.
	renderCharts: function (ui) {
		for (var panel in ui) {
			if (!ui.hasOwnProperty(panel))
				continue;
			// Ensure it has a chartType property and has C3 definitions
			if (!ui[panel].chartType || !ui[panel].plot.length)
				continue;

			// Grab the data for the selected panel
			var data = this.processChartData(GoAccess.getPanelData(panel).data);
			if (ui[panel].chartReverse)
				data = data.reverse();

			// Render given its type
			switch (ui[panel].chartType) {
			case 'area-spline':
				this.renderAreaSpline(panel, ui[panel].plot[0], data);
				break;
			case 'bar':
				this.renderVBar(panel, ui[panel].plot[0], data);
				break;
			};
		}
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
	events: function () {
		var _this = this;
		$$('.panel-next', function (item) {
			item.onclick = function (e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				_this.renderTable(panel, _this.nextPage(panel))
			};
		});

		$$('.panel-prev', function (item) {
			item.onclick = function (e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				_this.renderTable(panel, _this.prevPage(panel))
			};
		});

		$$('.row-collapsed.clickable', function (item) {
			item.onclick = function (e) {
				_this.toggleRow(e.currentTarget);
			};
		});
	},

	// Toggle children rows
	toggleRow: function (ele) {
		var cell = ele;
		cell.classList.toggle('row-expanded')
		var row = cell.parentNode;
		while (row = row.nextSibling) {
			if (row.tagName != 'TR')
				continue;
			if (row.className.indexOf('parent') != -1)
				break;
			row.classList.toggle('hide');
		}
	},

	// Get current panel page
	getCurPage: function (panel) {
		return GoAccess.getPanelUI(panel).curPage || 0;
	},

	// Page offset.
	// e.g., Return Value: 11, curPage: 2
	pageOffSet: function (panel) {
		var curPage = GoAccess.getPanelUI(panel).curPage || 0;
		return ((curPage - 1) * GoAccess.getPrefs().perPage);
	},

	// Get total number of pages given the number of items on array
	getTotalPages: function (dataItems) {
		return Math.ceil(dataItems.length / GoAccess.getPrefs().perPage);
	},

	// Get a shallow copy of a portion of the given data array and the
	// current page.
	getPage: function (panel, dataItems, page) {
		var ui = GoAccess.getPanelUI(panel);
		var totalPages = this.getTotalPages(dataItems);
		if (page < 1)
			page = 1;
		if (page > totalPages)
			page = totalPages;

		ui.curPage = page;
		var start = this.pageOffSet(panel);
		var end = start + GoAccess.getPrefs().perPage;

		return dataItems.slice(start, end);
	},

	// Get previous page
	prevPage: function (panel) {
		var curPage = GoAccess.getPanelUI(panel).curPage || 0;
		return curPage - 1;
	},

	// Get next page
	nextPage: function (panel) {
		var curPage = GoAccess.getPanelUI(panel).curPage || 0;
		return curPage + 1;
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
			'max'      : max != undefined ? GoAccess.Common.fmtValue(max, ui.valueType) : null,
			'min'      : min != undefined ? GoAccess.Common.fmtValue(min, ui.valueType) : null,
			'value'    : val != undefined ? GoAccess.Common.fmtValue(val, ui.valueType) : null,
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

		var template = $('#tpl-table-row-meta').innerHTML;
		table.innerHTML = Hogan.compile(template).render({
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
	getTableCell: function (panel, ui, value) {
		var className = ui.className || '';
		className += ui.valueType != 'string' ? 'text-right' : '';
		return {
			'className': className,
			'percent': GoAccess.Common.getPercent(value),
			'value': GoAccess.Common.fmtValue(GoAccess.Common.getCount(value), ui.valueType)
		};
	},

	// Given a data item object, set all the row cells and return a
	// table row that the template can consume.
	renderRow: function (panel, callback, ui, dataItem, idx, subItem, parentId) {
		var shadeParent = ((!subItem && idx % 2 != 0) ? 'shaded' : '');
		var shadeChild = ((parentId % 2 != 0) ? 'shaded' : '');
		return {
			'idx'         : !subItem && (String(idx + this.pageOffSet(panel))),
			'parentId'    : subItem ? String(parentId) : '',
			'className'   : subItem ? 'child hide ' + shadeChild : 'parent ' + shadeParent,
			'cells'       : this.iterUIItems(panel, ui.items, dataItem, callback),
			'hasSubItems' : ui.hasSubItems,
			'items'       : dataItem.items ? dataItem.items.length : 0,
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
		var callback = this.getTableCell.bind(this);
		for (var i = 0; i < dataItems.length; ++i) {
			var dataItem = dataItems[i];
			rows.push(this.renderRow(panel, callback, ui, dataItem, i, subItem, parentId));
			if (dataItem.items && dataItem.items.length) {
				this.renderRows(rows, panel, ui, dataItem.items, true, i);
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

		var template = $('#tpl-table-row').innerHTML;
		table.innerHTML = Hogan.compile(template).render({
			rows: rows
		});
	},

	// Iterate over all panels and determine which ones should contain
	// a data table.
	renderTables: function () {
		var ui = GoAccess.getPanelUI();
		for (var panel in ui) {
			if (GoAccess.Common.isPanelValid(panel))
				continue;

			// panel's data
			var data = GoAccess.getPanelData(panel);
			// render meta data
			if (data.hasOwnProperty('metadata'))
				this.renderMetaRow(panel, ui[panel]);
			// render actual data
			if (data.hasOwnProperty('data')) {
				this.renderDataRows(panel, ui[panel], data.data, 0);
			}
		}
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
	},

	initialize: function () {
		this.renderTables();
		this.events();
	},
};

// Main App
GoAccess.App = {
	initialize: function () {
		GoAccess.Nav.initialize();
		GoAccess.OverallStats.initialize();
		GoAccess.Panels.initialize();
		GoAccess.Charts.initialize();
		GoAccess.Tables.initialize();
	}
};

function AreaChart(dualYaxis) {
	var margin = {
			top    : 20,
			right  : 50,
			bottom : 40,
			left   : 50
		},
		width = 760,
		height = 170,
		nTicks = 10;
	var labels = { x: 'Unnamed', y0: 'Unnamed', y1: 'Unnamed' };
	var format = { x: null, y0: null, y1: null};

	var	xValue = function (d) {
			return d[0];
		},
		yValue0 = function (d) {
			return d[1];
		},
		yValue1 = function (d) {
			return d[2];
		};

	var xScale = d3.scale.ordinal();
	var yScale0 = d3.scale.linear().nice();
	var yScale1 = d3.scale.linear().nice();

	var xAxis = d3.svg.axis()
		.scale(xScale)
		.orient("bottom");

	var yAxis0 = d3.svg.axis()
		.scale(yScale0)
		.orient("left")
		.tickFormat(function (d) {
			if (format.y0)
				return GoAccess.Common.fmtValue(d, format.y0);
			return d3.format('.2s')(d);
		});

	var yAxis1 = d3.svg.axis()
		.scale(yScale1)
		.orient("right")
		.tickFormat(function (d) {
			if (format.y1)
				return GoAccess.Common.fmtValue(d, format.y1);
			return d3.format('.2s')(d);
		});

	var xGrid = d3.svg.axis()
		.scale(xScale)
		.orient("bottom");

	var yGrid = d3.svg.axis()
		.scale(yScale0)
		.orient("left");

	var area0 = d3.svg.area()
		.interpolate('cardinal')
		.x(X)
		.y(Y0);
	var area1 = d3.svg.area()
		.interpolate('cardinal')
		.x(X)
		.y(Y1);

	var line0 = d3.svg.line()
		.interpolate('cardinal')
		.x(X)
		.y(Y0);
	var line1 = d3.svg.line()
		.interpolate('cardinal')
		.x(X)
		.y(Y1);

	// The x-accessor for the path generator; xScale ∘ xValue.
	function X(d) {
		return xScale(d[0]);
	}

	// The x-accessor for the path generator; yScale0 yValue0.
	function Y0(d) {
		return yScale0(d[1]);
	}

	// The x-accessor for the path generator; yScale0 yValue0.
	function Y1(d) {
		return yScale1(d[2]);
	}

	function innerW() {
		return width - margin.left - margin.right;
	}

	function innerH() {
		return height - margin.top - margin.bottom;
	}

	function getXTicks(data) {
		if (data.length < nTicks)
			return xScale.domain();

		return d3.range(0, data.length, Math.round(data.length / nTicks)).map(function (d) {
			return xScale.domain()[d];
		});
	}

	function getYTicks(scale) {
		var domain = scale.domain();
		return d3.range(domain[0], domain[1], Math.ceil(domain[1] / nTicks));
	}

	// Convert data to standard representation greedily;
	// this is needed for nondeterministic accessors.
	function mapData(data) {
		var _datum = function (d, i) {
			var datum = [xValue.call(data, d, i), yValue0.call(data, d, i)];
			dualYaxis && datum.push(yValue1.call(data, d, i));
			return datum;
		};
		return data.map(function (d, i) {
			return _datum(d, i);
		});
	}

	function updateScales(data) {
		// Update the x-scale.
		xScale.domain(data.map(function (d) {
			return d[0];
		}))
		.rangePoints([0, innerW()], 1);

		// Update the y-scale.
		yScale0.domain([0, d3.max(data, function (d) {
			return d[1];
		})])
		.range([innerH(), 0]);

		// Update the y-scale.
		dualYaxis && yScale1.domain([0, d3.max(data, function (d) {
			return d[2];
		})])
		.range([innerH(), 0]);
	}

	function setLegendLabels(svg) {
		// Legend Color
		var rect = svg.selectAll('rect.legend.y0').data([null]);
		rect.enter().append("rect")
			.attr("class", "legend y0")
			.attr("y", (height - 15));
		rect
			.attr("x", (width / 2) - 100);

		// Legend Labels
		var text = svg.selectAll('text.legend.y0').data([null]);
		text.enter().append("text")
			.attr("class", "legend y0")
			.attr("y", (height - 6));
		text
			.attr("x", (width / 2) - 85)
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Legend Labels
		rect = svg.selectAll('rect.legend.y1').data([null]);
		rect.enter().append("rect")
			.attr("class", "legend y1")
			.attr("y", (height - 15));
		rect
			.attr("x", (width / 2));

		// Legend Labels
		text = svg.selectAll('text.legend.y1').data([null]);
		text.enter().append("text")
			.attr("class", "legend y1")
			.attr("y", (height - 6));
		text
			.attr("x", (width / 2) + 15)
			.text(labels.y1);
	}

	function setAxisLabels(svg) {
		// Labels
		svg.selectAll('text.axis-label.y0').data([null])
			.enter().append("text")
			.attr("class", "axis-label y0")
			.attr("y", 10)
			.attr("x", 50)
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Labels
		var tEnter = svg.selectAll('text.axis-label.y1').data([null]);
		tEnter.enter().append("text")
			.attr("class", "axis-label y1")
			.attr("y", 10)
			.text(labels.y1);
		dualYaxis && tEnter
			.attr("x", width - 25)
	}

	function createSkeleton(svg) {
		// Otherwise, create the skeletal chart.
		var gEnter = svg.enter().append("svg").append("g");

		// Lines
		gEnter.append("path")
			.attr("class", "line line0");
		dualYaxis && gEnter.append("path")
			.attr("class", "line line1");

		// Areas
		gEnter.append("path")
			.attr("class", "area area0");
		dualYaxis && gEnter.append("path")
			.attr("class", "area area1");

		// Points
		gEnter.append("g")
			.attr("class", "points y0");
		dualYaxis && gEnter.append("g")
			.attr("class", "points y1");

		// Grid
		gEnter.append("g")
			.attr("class", "x grid");
		gEnter.append("g")
			.attr("class", "y grid");

		// Axis
		gEnter.append("g")
			.attr("class", "x axis");
		gEnter.append("g")
			.attr("class", "y0 axis");
		dualYaxis && gEnter.append("g")
			.attr("class", "y1 axis");

		// Rects
		gEnter.append("g")
			.attr("class", "rects");

		setAxisLabels(svg);
		setLegendLabels(svg);

		// Mouseover line
		gEnter.append("line")
			.attr("y2", innerH())
			.attr("y1", 0)
			.attr("class", "indicator");
	}

	// Update the area path and lines.
	function addAreaLines(g) {
		// Update the area path.
		g.select(".area0").attr("d", area0.y0(yScale0.range()[0]));
		// Update the line path.
		g.select(".line0").attr("d", line0);

		if (!dualYaxis)
			return;

		// Update the area path.
		g.select(".area1").attr("d", area1.y1(yScale1.range()[0]));
		// Update the line path.
		g.select(".line1").attr("d", line1);
	}

	// Update chart points
	function addPoints(g, data) {
		var radius = data.length > 100 ? 1 : 2.5;

		var points = g.select('g.points.y0').selectAll('circle.point')
			.data(data);
		points
			.enter()
			.append('svg:circle')
			.attr('r', radius)
			.attr('class', 'point');
		points
			.attr('cx', function (d) { return xScale(d[0]) })
			.attr('cy', function (d) { return yScale0(d[1]) })
		// remove elements
		points.exit().remove();

		if (!dualYaxis)
			return;

		points = g.select('g.points.y1').selectAll('circle.point')
			.data(data);
		points
			.enter()
			.append('svg:circle')
			.attr('r', radius)
			.attr('class', 'point');
		points
			.attr('cx', function (d) { return xScale(d[0]) })
			.attr('cy', function (d) { return yScale1(d[2]) })
		// remove elements
		points.exit().remove();
	}

	function addAxis(g, data) {
		// Update the x-axis.
		g.select(".x.axis")
			.attr("transform", "translate(0," + yScale0.range()[0] + ")")
			.call(xAxis
				.tickValues(getXTicks(data))
			 );
		// Update the y0-axis.
		g.select(".y0.axis")
			.call(yAxis0
				.tickValues(getYTicks(yScale0))
			);

		if (!dualYaxis)
			return;

		// Update the y1-axis.
		g.select(".y1.axis")
			.attr("transform", "translate(" + innerW() + ", 0)")
			.call(yAxis1
				.tickValues(getYTicks(yScale1))
			);
	}

	// Update the X-Y grid.
	function addGrid(g, data) {
		g.select(".x.grid")
			.attr("transform", "translate(0," + yScale0.range()[0] + ")")
			.call(xGrid
				.tickValues(getXTicks(data))
				.tickSize(-innerH(), 0, 0)
				.tickFormat("")
			);

		g.select(".y.grid")
			.call(yGrid
				.tickValues(getYTicks(yScale0))
				.tickSize(-innerW(), 0, 0)
				.tickFormat("")
			);
	}

	function formatTooltip(data, i) {
		var d = data.slice(0);

		d[0] = (format.x) ? GoAccess.Common.fmtValue(d[0], format.x) : d[0];
		d[1] = (format.y0) ? GoAccess.Common.fmtValue(d[1], format.y0) : d3.format(',')(d[1]);
		dualYaxis && (d[2] = (format.y1) ? GoAccess.Common.fmtValue(d[2], format.y1) : d3.format(',')(d[2]));

		var template = d3.select('#tpl-chart-tooltip').html();
		return Hogan.compile(template).render({
			'data': d
		});
	}

	function mouseover(_self, selection, data, idx) {
		var tooltip = selection.select(".chart-tooltip-wrap");
		tooltip.html(formatTooltip(data, idx))
			.style('left', (xScale(data[0])) + 'px')
			.style('top',  (d3.mouse(_self)[1] + 10) + "px")
			.style('display', 'block');

		selection.select("line.indicator")
			.style("display", "block")
			.attr("transform", "translate(" + xScale(data[0]) + "," + 0 + ")");
	}

	function mouseout(selection, g) {
		var tooltip = selection.select(".chart-tooltip-wrap");
		tooltip.style('display', 'none');

		g.select("line.indicator").style("display", "none");
	}

	function addRects(selection, g, data) {
		var w = (innerW() / data.length);

		var rects = g.select("g.rects").selectAll("rect")
			.data(data);
		rects
			.enter()
			.append('svg:rect')
			.attr('height', innerH())
			.attr('class', 'point');
		rects
			.attr('width', d3.functor(w))
			.attr('x', function (d, i) { return (w * i); })
			.attr('y', 0)
			.on('mousemove', function (d, i) {
				mouseover(this, selection, d, i);
			})
			.on('mouseleave', function (d, i) {
				mouseout(selection, g);
			});
		// remove elements
		rects.exit().remove();
	}

	function chart(selection) {
		selection.each(function (data) {
			// normalize data
			data = mapData(data);
			// updates X-Y scales
			updateScales(data);

			// Select the svg element, if it exists.
			var svg = d3.select(this).selectAll("svg").data([data]);
			createSkeleton(svg);

			// Update the outer dimensions.
			svg.attr({
				"width": width,
				"height": height
			});

			// Update the inner dimensions.
			var g = svg.select("g")
				.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

			// Add grid
			addGrid(g, data);
			// Add chart lines and areas
			addAreaLines(g);
			// Add chart points
			addPoints(g, data);
			// Add axis
			addAxis(g, data);
			// Add rects
			addRects(selection, g, data);
		});
	}

	chart.format = function (_) {
		if (!arguments.length) return format;
		format = _;
		return chart;
	};

	chart.labels = function (_) {
		if (!arguments.length) return labels;
		labels = _;
		return chart;
	};

	chart.margin = function (_) {
		if (!arguments.length) return margin;
		margin = _;
		return chart;
	};

	chart.width = function (_) {
		if (!arguments.length) return width;
		width = _;
		return chart;
	};

	chart.height = function (_) {
		if (!arguments.length) return height;
		height = _;
		return chart;
	};

	chart.x = function (_) {
		if (!arguments.length) return xValue;
		xValue = _;
		return chart;
	};

	chart.y0 = function (_) {
		if (!arguments.length) return yValue0;
		yValue0 = _;
		return chart;
	};

	chart.y1 = function (_) {
		if (!arguments.length) return yValue1;
		yValue1 = _;
		return chart;
	};

	return chart;
}

function BarChart(dualYaxis) {
	var margin = {
			top    : 20,
			right  : 50,
			bottom : 40,
			left   : 50
		},
		width = 760,
		height = 170,
		nTicks = 10;
	var labels = { x: 'Unnamed', y0: 'Unnamed', y1: 'Unnamed' };
	var format = { x: null, y0: null, y1: null};

	var	xValue = function (d) {
			return d[0];
		},
		yValue0 = function (d) {
			return d[1];
		},
		yValue1 = function (d) {
			return d[2];
		};

	var xScale = d3.scale.ordinal();
	var yScale0 = d3.scale.linear().nice();
	var yScale1 = d3.scale.linear().nice();

	var xAxis = d3.svg.axis()
		.scale(xScale)
		.orient("bottom");

	var yAxis0 = d3.svg.axis()
		.scale(yScale0)
		.orient("left")
		.tickFormat(function (d) {
			if (format.y1)
				return GoAccess.Common.fmtValue(d, format.y1);
			return d3.format('.2s')(d);
		});

	var yAxis1 = d3.svg.axis()
		.scale(yScale1)
		.orient("right")
		.tickFormat(function (d) {
			if (format.y1)
				return GoAccess.Common.fmtValue(d, format.y1);
			return d3.format('.2s')(d);
		});

	var xGrid = d3.svg.axis()
		.scale(xScale)
		.orient("bottom");

	var yGrid = d3.svg.axis()
		.scale(yScale0)
		.orient("left");

	// The x-accessor for the path generator; xScale ∘ xValue.
	function X(d) {
		return xScale(d[0]);
	}

	// The x-accessor for the path generator; yScale0 yValue0.
	function Y0(d) {
		return yScale0(d[1]);
	}

	// The x-accessor for the path generator; yScale0 yValue0.
	function Y1(d) {
		return yScale1(d[2]);
	}

	function innerW() {
		return width - margin.left - margin.right;
	}

	function innerH() {
		return height - margin.top - margin.bottom;
	}

	function getXTicks(data) {
		if (data.length < nTicks)
			return xScale.domain();

		return d3.range(0, data.length, Math.round(data.length / nTicks)).map(function (d) {
			return xScale.domain()[d];
		});
	}

	function getYTicks(scale) {
		var domain = scale.domain();
		return d3.range(domain[0], domain[1], Math.ceil(domain[1] / nTicks));
	}

	// Convert data to standard representation greedily;
	// this is needed for nondeterministic accessors.
	function mapData(data) {
		var _datum = function (d, i) {
			var datum = [xValue.call(data, d, i), yValue0.call(data, d, i)];
			dualYaxis && datum.push(yValue1.call(data, d, i));
			return datum;
		};
		return data.map(function (d, i) {
			return _datum(d, i);
		});
	}

	function updateScales(data) {
		// Update the x-scale.
		xScale.domain(data.map(function (d) {
			return d[0];
		}))
		.rangeRoundBands([0, innerW()], .1);

		// Update the y-scale.
		yScale0.domain([0, d3.max(data, function (d) {
			return d[1];
		})])
		.range([innerH(), 0]);

		// Update the y-scale.
		dualYaxis && yScale1.domain([0, d3.max(data, function (d) {
			return d[2];
		})])
		.range([innerH(), 0]);
	}

	function setLegendLabels(svg) {
		// Legend Color
		var rect = svg.selectAll('rect.legend.y0').data([null]);
		rect.enter().append("rect")
			.attr("class", "legend y0")
			.attr("y", (height - 15));
		rect
			.attr("x", (width / 2) - 100);

		// Legend Labels
		var text = svg.selectAll('text.legend.y0').data([null]);
		text.enter().append("text")
			.attr("class", "legend y0")
			.attr("y", (height - 6));
		text
			.attr("x", (width / 2) - 85)
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Legend Labels
		rect = svg.selectAll('rect.legend.y1').data([null]);
		rect.enter().append("rect")
			.attr("class", "legend y1")
			.attr("y", (height - 15));
		rect
			.attr("x", (width / 2));

		// Legend Labels
		text = svg.selectAll('text.legend.y1').data([null]);
		text.enter().append("text")
			.attr("class", "legend y1")
			.attr("y", (height - 6));
		text
			.attr("x", (width / 2) + 15)
			.text(labels.y1);
	}

	function setAxisLabels(svg) {
		// Labels
		svg.selectAll('text.axis-label.y0').data([null])
			.enter().append("text")
			.attr("class", "axis-label y0")
			.attr("y", 10)
			.attr("x", 50)
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Labels
		var tEnter = svg.selectAll('text.axis-label.y1').data([null]);
		tEnter.enter().append("text")
			.attr("class", "axis-label y1")
			.attr("y", 10)
			.text(labels.y1);
		dualYaxis && tEnter
			.attr("x", width - 25)
	}

	function createSkeleton(svg) {
		// Otherwise, create the skeletal chart.
		var gEnter = svg.enter().append("svg").append("g");

		// Grid
		gEnter.append("g")
			.attr("class", "x grid");
		gEnter.append("g")
			.attr("class", "y grid");

		// Axis
		gEnter.append("g")
			.attr("class", "x axis");
		gEnter.append("g")
			.attr("class", "y0 axis");
		dualYaxis && gEnter.append("g")
			.attr("class", "y1 axis");

		// Bars
		gEnter.append("g")
			.attr("class", "bars y0");
		dualYaxis && gEnter.append("g")
			.attr("class", "bars y1");

		// Rects
		gEnter.append("g")
			.attr("class", "rects");

		setAxisLabels(svg);
		setLegendLabels(svg);

		// Mouseover line
		gEnter.append("line")
			.attr("y2", innerH())
			.attr("y1", 0)
			.attr("class", "indicator");
	}

	// Update the area path and lines.
	function addBars(g, data) {
		var bars = g.select('g.bars.y0').selectAll('rect.bar')
			.data(data);
		bars
			.enter()
			.append('svg:rect')
			.attr('class', 'bar');
		bars
			.attr('width', xScale.rangeBand() / 2)
			.attr('height', function (d) { return innerH() - yScale0(d[1]) })
			.attr('x', function (d) { return xScale(d[0]) })
			.attr('y', function (d) { return yScale0(d[1]) });
		// remove elements
		bars.exit().remove();

		if (!dualYaxis)
			return;

		var bars = g.select('g.bars.y1').selectAll('rect.bar')
			.data(data);
		bars
			.enter()
			.append('svg:rect')
			.attr('class', 'bar');
		bars
			.attr('width', xScale.rangeBand() / 2)
			.attr('height', function (d) { return innerH() - yScale1(d[2]) })
			.attr('x', function (d) { return (xScale(d[0]) + xScale.rangeBand() / 2) })
			.attr('y', function (d) { return yScale1(d[2]) });
		// remove elements
		bars.exit().remove();
	}

	function addAxis(g, data) {
		// Update the x-axis.
		g.select(".x.axis")
			.attr("transform", "translate(0," + yScale0.range()[0] + ")")
			.call(xAxis
				.tickValues(getXTicks(data))
			 );
		// Update the y0-axis.
		g.select(".y0.axis")
			.call(yAxis0
				.tickValues(getYTicks(yScale0))
			);

		if (!dualYaxis)
			return;

		// Update the y1-axis.
		g.select(".y1.axis")
			.attr("transform", "translate(" + innerW() + ", 0)")
			.call(yAxis1
				.tickValues(getYTicks(yScale1))
			);
	}

	// Update the X-Y grid.
	function addGrid(g, data) {
		g.select(".x.grid")
			.attr("transform", "translate(0," + yScale0.range()[0] + ")")
			.call(xGrid
				.tickValues(getXTicks(data))
				.tickSize(-innerH(), 0, 0)
				.tickFormat("")
			);

		g.select(".y.grid")
			.call(yGrid
				.tickValues(getYTicks(yScale0))
				.tickSize(-innerW(), 0, 0)
				.tickFormat("")
			);
	}

	function formatTooltip(data, i) {
		var d = data.slice(0);

		d[0] = (format.x) ? GoAccess.Common.fmtValue(d[0], format.x) : d[0];
		d[1] = (format.y0) ? GoAccess.Common.fmtValue(d[1], format.y0) : d3.format(',')(d[1]);
		dualYaxis && (d[2] = (format.y1) ? GoAccess.Common.fmtValue(d[2], format.y1) : d3.format(',')(d[2]));

		var template = d3.select('#tpl-chart-tooltip').html();
		return Hogan.compile(template).render({
			'data': d
		});
	}

	function mouseover(_self, selection, data, idx) {
		var tooltip = selection.select(".chart-tooltip-wrap");
		tooltip.html(formatTooltip(data, idx))
			.style('left', (xScale(data[0])) + 'px')
			.style('top',  (d3.mouse(_self)[1] + 10) + "px")
			.style('display', 'block');

		selection.select("line.indicator")
			.style("display", "block")
			.attr("transform", "translate(" + xScale(data[0]) + "," + 0 + ")");
	}

	function mouseout(selection, g) {
		var tooltip = selection.select(".chart-tooltip-wrap");
		tooltip.style('display', 'none');

		g.select("line.indicator").style("display", "none");
	}

	function addRects(selection, g, data) {
		var w = (innerW() / data.length);

		var rects = g.select("g.rects").selectAll("rect")
			.data(data);
		rects
			.enter()
			.append('svg:rect')
			.attr('height', innerH())
			.attr('class', 'point');
		rects
			.attr('width', d3.functor(w))
			.attr('x', function (d, i) { return (w * i); })
			.attr('y', 0)
			.on('mousemove', function (d, i) {
				mouseover(this, selection, d, i);
			})
			.on('mouseleave', function (d, i) {
				mouseout(selection, g);
			});
		// remove elements
		rects.exit().remove();
	}

	function chart(selection) {
		selection.each(function (data) {
			// normalize data
			data = mapData(data);
			// updates X-Y scales
			updateScales(data);

			// Select the svg element, if it exists.
			var svg = d3.select(this).selectAll("svg").data([data]);
			createSkeleton(svg);

			// Update the outer dimensions.
			svg.attr({
				"width": width,
				"height": height
			});

			// Update the inner dimensions.
			var g = svg.select("g")
				.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

			// Add grid
			addGrid(g, data);
			// Add axis
			addAxis(g, data);
			// Add chart lines and areas
			addBars(g, data);
			// Add rects
			addRects(selection, g, data);
		});
	}

	chart.format = function (_) {
		if (!arguments.length) return format;
		format = _;
		return chart;
	};

	chart.labels = function (_) {
		if (!arguments.length) return labels;
		labels = _;
		return chart;
	};

	chart.width = function (_) {
		if (!arguments.length) return width;
		width = _;
		return chart;
	};

	chart.height = function (_) {
		if (!arguments.length) return height;
		height = _;
		return chart;
	};

	chart.x = function (_) {
		if (!arguments.length) return xValue;
		xValue = _;
		return chart;
	};

	chart.y0 = function (_) {
		if (!arguments.length) return yValue0;
		yValue0 = _;
		return chart;
	};

	chart.y1 = function (_) {
		if (!arguments.length) return yValue1;
		yValue1 = _;
		return chart;
	};

	return chart;
}

// Init app
window.onload = function () {
	GoAccess.initialize({
		'uiData': user_interface,
		'panelData': json_data
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
		evTarget.parentElement.classList.remove('open');

		// Trigger the click event on the target if it not opening another menu
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
