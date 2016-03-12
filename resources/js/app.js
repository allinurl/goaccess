var GoAccess = (function() {
	var AppCharts = {}, // holds all rendered charts
		AppUIData = {}, // holds panel definitions
		AppData = {}, // hold raw data
		AppPrefs = {
			'perPage': 11, // panel rows per page
		};

	// Helpers
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

	// Add all attributes of n to o
	function merge(o, n) {
		for (var attrname in n) {
			o[attrname] = n[attrname];
		}
	}

	// Format bytes to human readable
	function formatBytes(bytes, decimals) {
		if (bytes == 0) return '0 Byte';
		var k = 1024;
		var dm = decimals + 1 || 3;
		var sizes = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
		var i = Math.floor(Math.log(bytes) / Math.log(k));
		return (bytes / Math.pow(k, i)).toPrecision(dm) + ' ' + sizes[i];
	}

	// Validate number
	function isNumeric(n) {
		return !isNaN(parseFloat(n)) && isFinite(n);
	}

	// Format microseconds to human readable
	function utime2str(usec) {
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
	}

	function formatDate(value) {
		var months = ["Jan", "Feb", "Ma", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
		var d = parseInt(value);
		d = new Date(d / 1E4, (d % 1E4 / 100) - 1, d % 100);
		return ('0' + d.getDate()).slice(-2) + '/' + months[d.getMonth()] + '/' + d.getFullYear();
	}

	// Format field value to human readable
	function fmtValue(value, valueType) {
		var val = 0;
		if (!valueType)
			val = value;

		switch (valueType) {
		case 'numeric':
			if (isNumeric(value))
				val = value.toLocaleString();
			break;
		case 'time':
			if (isNumeric(value))
				val = value.toLocaleString();
			break;
		case 'bytes':
			val = formatBytes(value);
			break;
		case 'percent':
			val = value.toFixed(2) + '%';
			break;
		case 'utime':
			val = utime2str(value);
			break;
		case 'date':
			val = formatDate(value);
			break;
		default:
			val = value;
		};

		return value == 0 ? String(val) : val;
	}

	// Get JSON data for the given panel
	function getData(panel) {
		return panel ? AppData[panel] : AppData;
	}

	// Get user interface definition for the given panel
	function getUIData(panel) {
		return panel ? AppUIData[panel] : AppUIData;
	}

	/**
	 * A panel must exist in AppData and in AppUIData. Furthermore it must not have an
	 * empty id to marked as valid.
	 * @param  {string}  panel Name of panel
	 * @return {Boolean}       Validity of panel
	 */
	function isPanelValid(panel) {
		if (!AppUIData.hasOwnProperty(panel) || !AppData.hasOwnProperty(panel) || !AppUIData[panel].id)
			return true;
		else
			return false;
	}

	// Render general/overall analyzed requests.
	function renderGeneral() {
		var ui = getUIData('general');
		var data = getData('general');

		// General section wrapper
		var template = $('#tpl-general').innerHTML;
		$('.wrap-general').innerHTML = Hogan.compile(template).render(ui);

		var i = 0;
		var template = $('#tpl-general-items').innerHTML;
		var wrap = $('.wrap-general-items');

		// Iterate over general data object
		for (var x in data) {
			if (!data.hasOwnProperty(x) || !ui.items.hasOwnProperty(x))
				continue;

			// create a new bootstrap row every 6 elements
			if (i % 6 == 0) {
				var row = document.createElement('div');
				row.setAttribute('class', 'row');
				wrap.appendChild(row);
			}

			var box = document.createElement('div');
			box.innerHTML = Hogan.compile(template).render({
				'className': ui.items[x].className,
				'label': ui.items[x].label,
				'value': fmtValue(data[x], ui.items[x].valueType),
			});
			row.appendChild(box);
			i++;
		}
	}

	// Redraw a chart upon selecting a metric.
	function redrawChart(targ) {
		var plot = targ.getAttribute('data-plot');
		var panel = targ.getAttribute('data-panel');
		var ui = getUIData(panel);
		var plotUI = ui.plot;

		// Iterate over plot user interface definition
		for (var x in plotUI) {
			if (!plotUI.hasOwnProperty(x) || plotUI[x].className != plot)
				continue;

			// Extract data for the selected panel and process it
			var data = processChartData(getData(panel).data);
			if (ui.chartReverse)
				data = data.reverse();

			d3.select('#chart-' + panel).select('svg').remove();
			renderAreaSpline(panel, plotUI[x], data);
			break;
		}
	}

	// Set panel to either expanded or not expanded within the panel
	// definition.
	function setPanelExpanded(panel, targ) {
		var ui = getUIData(panel);
		if (targ.getAttribute('data-state') == 'collapsed')
			ui['expanded'] = true;
		else
			ui['expanded'] = false;
	}

	// Panel event handlers.
	function ePanelHandlers() {
		var _self = this;
		var plotOpts = document.querySelectorAll('[data-plot]');

		$$('.panel-next', function(item) {
			item.onclick = function(e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				renderTable(panel, nextPage(panel))
			};
		});

		$$('.panel-prev', function(item) {
			item.onclick = function(e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				renderTable(panel, prevPage(panel))
			};
		});

		$$('.panel-expand', function(item) {
			item.onclick = function(e) {
				var panel = e.currentTarget.getAttribute('data-panel');
				setPanelExpanded(panel, e.currentTarget);
				renderTable(panel, getCurPage(panel))
			};
		});

		$$('[data-plot]', function(item) {
			item.onclick = function(e) {
				var targ = e.currentTarget;
				redrawChart(targ);
			};
		});
	}

	// Render the given panel given a user interface definition.
	function renderPanel(panel, ui) {
		var template = $('#tpl-panel').innerHTML;
		var box = document.createElement('div');
		box.id = 'panel-' + panel;
		box.innerHTML = Hogan.compile(template).render(ui);

		var pagination = box.getElementsByClassName('pagination');
		pagination[0].getElementsByClassName('panel-prev')[0].parentNode.className = 'disabled';

		// Remove pagination if it's not needed
		if (ui['totalItems'] <= AppPrefs['perPage'])
			pagination[0].parentNode.removeChild(pagination[0]);

		$('.wrap-panels').appendChild(box);
	}

	// Render left-hand side navigation given the available panels.
	function renderNav(nav) {
		var template = $('#tpl-panel-nav').innerHTML;
		$('.panel-nav').innerHTML = Hogan.compile(template).render({
			nav: nav
		});
	}

	// Iterate over all available panels and render each.
	function renderPanels() {
		var ui = getUIData();

		var nav = [];
		for (var panel in ui) {
			if (isPanelValid(panel))
				continue;
			// Push panel to our navigation array
			nav.push({
				'key': panel,
				'head': ui[panel].head,
			});
			// Render panel given a user interface definition
			renderPanel(panel, ui[panel]);
		}
		renderNav(nav);
		ePanelHandlers();
	}

	// Set default C3 chart to area spline and apply panel user interface
	// definition and load data.
	function renderAreaSpline(panel, plotData, data) {
		// data.forEach(function(d) {
		// 	var date = new Date(d.data / 1E4 , (d.data % 1E4 / 100) - 1, d.data % 100);
		// 	d.data = d3.time.format('%d/%b/%Y')(date);
		// });

		var dualYaxis = plotData['d3']['y1'];
		var chart = AreaChart(dualYaxis)
			.width($("#chart-" + panel).offsetWidth)
			.height(170)
			.labels({
				y0: plotData['d3']['y0'].label,
				y1: dualYaxis ? plotData['d3']['y1'].label : ''
			})
			.x(function(d) {
				return d.data;;
			})
			.y0(function(d) {
				return +d[plotData['d3']['y0']['key']];
			});

		dualYaxis && chart.y1(function(d) {
			return +d[plotData['d3']['y1']['key']];
		});

		d3.select("#chart-" + panel)
			.datum(data)
			.call(chart)
			.append("div").attr("class", "chart-tooltip-wrap");

		// Update the data for #example1, without appending another <svg> element
		// var a = setInterval(function () {
		// 	data.push({'data': '20101219', 'hits': 30000, 'visitors': 2300});
		// 	d3.select("#chart-" + panel)
		// 		.datum(data)
		// 		.call(chart);
		// 	clearInterval(a)
		// }, 5000);

		AppCharts[panel] = chart;
	}

	// Set default C3 chart to area spline and apply panel user interface
	// definition and load data.
	function renderVBar(panel, plotData, data) {
		// data.forEach(function(d) {
		// 	var date = new Date(d.data / 1E4 , (d.data % 1E4 / 100) - 1, d.data % 100);
		// 	d.data = d3.time.format('%d/%b/%Y')(date);
		// });

		var dualYaxis = plotData['d3']['y1'];
		var chart = BarChart(dualYaxis)
			.width($("#chart-" + panel).offsetWidth)
			.height(170)
			.labels({
				y0: plotData['d3']['y0'].label,
				y1: dualYaxis ? plotData['d3']['y1'].label : ''
			})
			.x(function(d) {
				return d.data;;
			})
			.y0(function(d) {
				return +d[plotData['d3']['y0']['key']];
			});

		dualYaxis && chart.y1(function(d) {
			return +d[plotData['d3']['y1']['key']];
		});

		d3.select("#chart-" + panel)
			.datum(data)
			.call(chart)
			.append("div").attr("class", "chart-tooltip-wrap");

		AppCharts[panel] = chart;
	}

	// Render all charts for the applicable panels.
	function renderCharts() {
		var ui = getUIData();
		for (var panel in ui) {
			if (!ui.hasOwnProperty(panel))
				continue;
			// Ensure it has a chartType property and has C3 definitions
			if (!ui[panel].chartType || !ui[panel].plot.length)
				continue;

			// Grab the data for the selected panel
			var data = processChartData(getData(panel).data);
			if (ui[panel].chartReverse)
				data = data.reverse();

			// Render given its type
			switch (ui[panel].chartType) {
			case 'area-spline':
				renderAreaSpline(panel, ui[panel].plot[0], data);
				break;
			case 'bar':
				renderVBar(panel, ui[panel].plot[0], data);
				break;
			};
		}

		// Resize charts
		d3.select(window).on('resize', function () {
			Object.keys(AppCharts).forEach(function(panel) {
				d3.select("#chart-" + panel)
					.call(AppCharts[panel].width($("#chart-" + panel).offsetWidth));
			});
		});
	};

	// Extract an array of objects that C3 can consume to process the chart.
	// e.g., o = Object {hits: 37402, visitors: 6949, bytes: 505881789, avgts:
	// 118609, cumts: 4436224010…}
	function processChartData(data) {
		var out = [];
		for (var i = 0; i < data.length; ++i) {
			out.push(extractCount(data[i]));
		}
		return out;
	}

	function getPercent(item) {
		if (typeof item == 'object' && 'percent' in item)
			return fmtValue(item.percent, 'percent');
		return null;
	}

	// Attempts to extract the count from either an object or the actual value.
	// e.g., item = Object {count: 14351, percent: 5.79} OR item = 4824825140
	function getCount(item) {
		if (typeof item == 'object' && 'count' in item)
			return item.count;
		return item;
	}

	// Iterate over the item properties and and extract the count value.
	function extractCount(item) {
		var o = {};
		for (var prop in item) {
			o[prop] = getCount(item[prop]);
		}
		return o;
	}

	// Return an object that can be consumed by the table template given a user
	// interface definition and a cell value object.
	// e.g., value = Object {count: 14351, percent: 5.79}
	function getTableCell(panel, ui, value) {
		var className = ui.className || '';
		className += ui.valueType != 'string' ? 'text-right' : '';
		return {
			'className': className,
			'percent': getPercent(value),
			'value': fmtValue(getCount(value), ui.valueType)
		};
	}

	// Iterate over user interface definition properties
	function iterUIItems(panel, uiItems, dataItems, callback) {
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
	}

	// Get current panel page
	function getCurPage(panel) {
		return getUIData(panel).curPage || 0;
	}

	// Page offset.
	// e.g., Return Value: 11, curPage: 2
	function pageOffSet(panel) {
		var curPage = getUIData(panel).curPage || 0;
		return ((curPage - 1) * AppPrefs.perPage);
	}

	// Get total number of pages given the number of items on array
	function getTotalPages(dataItems) {
		return Math.ceil(dataItems.length / AppPrefs.perPage);
	}

	// Get a shallow copy of a portion of the given data array and the current
	// page.
	function getPage(panel, dataItems, page) {
		var ui = getUIData(panel);
		var totalPages = getTotalPages(dataItems);
		if (page < 1)
			page = 1;
		if (page > totalPages)
			page = totalPages;

		ui.curPage = page;
		var start = pageOffSet(panel);
		var end = start + AppPrefs.perPage;

		return dataItems.slice(start, end);
	}

	// Get previous page
	function prevPage(panel) {
		var curPage = getUIData(panel).curPage || 0;
		return curPage - 1;
	}

	// Get next page
	function nextPage(panel) {
		var curPage = getUIData(panel).curPage || 0;
		return curPage + 1;
	}

	// Render each data row into the table
	function renderRows(panel, uiItems, dataItems) {
		var rows = [];

		if (dataItems.length == 0 && uiItems.length) {
			rows.push({
				cells: [{
					className: 'text-center',
					colspan: uiItems.length + 1,
					value: 'No data on this panel.'
				}]
			});
		}

		// Iterate over all data items for the given panel and generate a table
		// row per date item.
		for (var i = 0; i < dataItems.length; i++) {
			rows.push({
				'subitem': dataItems[i].subitem,
				'className': '',
				'idx': i + pageOffSet(panel),
				'cells': iterUIItems(panel, uiItems, dataItems[i], getTableCell)
			});
		}

		return rows;
	}

	// Entry point to render all data rows into the table
	function renderDataRows(panel, uiItems, dataItems, page) {
		// find the table to set
		var table = $('.table-' + panel + ' tbody.tbody-data');
		if (!table)
			return;

		var dataItems = getPage(panel, dataItems, page);
		var rows = renderRows(panel, uiItems, dataItems);
		if (rows.length == 0)
			return;

		var template = $('#tpl-table-row').innerHTML;
		table.innerHTML = Hogan.compile(template).render({
			rows: rows
		});
	}

	// Get the meta data value for the given user interface key
	function getMetaValue(ui, value) {
		if ('meta' in ui)
			return value[ui.meta];
		return null;
	}

	// Return an object that can be consumed by the table template given a user
	// interface definition and a meta value object.
	// e.g., value = Object {count: 22953, max: 15, min: 10}
	function getMetaCell(ui, value) {
		var val = getMetaValue(ui, value);
		var max = (value || {}).max;
		var min = (value || {}).min;

		var className = ui.className || '';
		className += ui.valueType != 'string' ? 'text-right' : '';
		return {
			'className': className,
			'title': ui.meta,
			'max': max != undefined ? fmtValue(max, ui.valueType) : null,
			'min': min != undefined ? fmtValue(min, ui.valueType) : null,
			'value': val ? fmtValue(val, ui.valueType) : null
		}
	}

	// Iterate over all meta rows and render its data above each data column
	// i.e., "table header"
	function renderMetaRow(panel, uiItems) {
		// find the table to set
		var table = $('.table-' + panel + ' tbody.tbody-meta');
		if (!table)
			return;

		var cells = [];
		var data = getData(panel).metadata;
		for (var i = 0; i < uiItems.length; ++i) {
			var item = uiItems[i];
			var value = data[item.key];
			cells.push(getMetaCell(item, value))
		}

		var template = $('#tpl-table-row-meta').innerHTML;
		table.innerHTML = Hogan.compile(template).render({
			row: [{
				'cells': cells
			}]
		});
	}

	// Set general user interface options
	function setUIOpts(panel, ui) {
		var dataItems = getData(panel).data;

		// expandable panel?
		ui['expandable'] = dataItems.length && dataItems[0].items ? true : false;
		// pagination
		ui['curPage'] = 0;
		ui['totalItems'] = dataItems.length;
	}


	// Iterate over all data sub items. e.g., Ubuntu, Arch, etc.
	function getAllDataItems(panel) {
		var data = JSON.parse(JSON.stringify(getData(panel)));
		var dataItems = data.data.slice(),
			out = [];

		for (var i = 0; i < dataItems.length; ++i) {
			out.push(dataItems[i]);

			var items = dataItems[i].items.slice();
			for (var j = 0; j < items.length; ++j) {
				var subitem = items[j];
				subitem['subitem'] = true;
				out.push(subitem);
			}
			delete out[i].items;
		}

		return out;
	}

	// Render a table for the given panel.
	function renderTable(panel, page, expanded) {
		var dataItems = getData(panel).data;
		var ui = getUIData(panel);
		var panelHtml = $('#panel-' + panel);

		// data items for an expanded panel
		if (ui.expanded)
			dataItems = getAllDataItems(panel);

		if (panelHtml.getElementsByClassName('pagination')[0]) {
			// Enable all pagination buttons
			panelHtml.getElementsByClassName('panel-next')[0].parentNode.className = '';
			panelHtml.getElementsByClassName('panel-prev')[0].parentNode.className = '';

			// Diable pagination next button if last page is reached
			if (page >= getTotalPages(dataItems))
				panelHtml.getElementsByClassName('panel-next')[0].parentNode.className = 'disabled';

			// Disable pagination prev button if first page is reached
			if (page <= 1)
				panelHtml.getElementsByClassName('panel-prev')[0].parentNode.className = 'disabled';
		}

		// Render data rows
		renderDataRows(panel, ui.items, dataItems, page);
	}

	// Iterate over all panels and determine which ones should contain a data
	// table.
	function renderTables() {
		var ui = getUIData();

		for (var panel in ui) {
			if (isPanelValid(panel))
				continue;

			// panel's user interface definition
			var uiItems = ui[panel].items;
			// panel's data
			var data = getData(panel);
			// render meta data
			if (data.hasOwnProperty('metadata'))
				renderMetaRow(panel, uiItems);
			// render actual data
			if (data.hasOwnProperty('data'))
				renderDataRows(panel, uiItems, data.data, 0);
		}
	}

	// Attempt to render the layout (if applicable)
	function render() {
		renderGeneral();
		renderPanels();
		renderCharts();
		renderTables();
	}

	// App initialization i.e., set some basic stuff
	function initialize(options) {
		AppUIData = (options || {}).uidata;;
		AppData = (options || {}).data;;

		for (var panel in AppUIData) {
			if (isPanelValid(panel))
				continue;
			setUIOpts(panel, AppUIData[panel]);
		}

		render();
	}

	// Public functions
	return {
		start: initialize,
		format: fmtValue,
	};
})();

function AreaChart(dualYaxis) {
	var margin = {
			top    : 10,
			right  : 50,
			bottom : 40,
			left   : 50
		},
		width = 760,
		height = 170,
		nTicks = 10;
	var labels = { x: 'Unnamed', y0: 'Unnamed', y1: 'Unnamed' };

	var	xValue = function(d) {
			return d[0];
		},
		yValue0 = function(d) {
			return d[1];
		},
		yValue1 = function(d) {
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
		.ticks(10, "s");

	var yAxis1 = d3.svg.axis()
		.scale(yScale1)
		.orient("right")
		.ticks(10, "s");

	var xGrid = d3.svg.axis()
		.scale(xScale)
		.orient("bottom");

	var yGrid = d3.svg.axis()
		.scale(yScale0)
		.orient("left")
		.ticks(10, "s");

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

		return d3.range(0, data.length, Math.round(data.length / nTicks)).map(function(d) {
			return xScale.domain()[d];
		});
	}

	// Convert data to standard representation greedily;
	// this is needed for nondeterministic accessors.
	function mapData(data) {
		var _datum = function(d, i) {
			var datum = [xValue.call(data, d, i), yValue0.call(data, d, i)];
			dualYaxis && datum.push(yValue1.call(data, d, i));
			return datum;
		};
		return data.map(function(d, i) {
			return _datum(d, i);
		});
	}

	function updateScales(data) {
		// Update the x-scale.
		xScale.domain(data.map(function(d) {
			return d[0];
		}))
		.rangePoints([0, innerW()], 1);

		// Update the y-scale.
		yScale0.domain([0, d3.max(data, function(d) {
			return d[1];
		})])
		.range([innerH(), 0]);

		// Update the y-scale.
		dualYaxis && yScale1.domain([0, d3.max(data, function(d) {
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
			.attr("y", 6)
			.attr("x", -10)
			.attr("dy", ".75em")
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Labels
		var tEnter = svg.selectAll('text.axis-label.y1').data([null]);
		tEnter.enter().append("text")
			.attr("class", "axis-label y1")
			.attr("x", -10)
			.attr("dy", ".75em")
			.text(labels.y1);
		dualYaxis && tEnter
			.attr("y", width - 15)
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
		var points = g.select('g.points.y0').selectAll('circle.point')
			.data(data);
		points
			.enter()
			.append('svg:circle')
			.attr('r', 2.5)
			.attr('class', 'point');
		points
			.attr('cx', function(d) { return xScale(d[0]) })
			.attr('cy', function(d) { return yScale0(d[1]) })
		// remove elements
		points.exit().remove();

		if (!dualYaxis)
			return;

		points = g.select('g.points.y1').selectAll('circle.point')
			.data(data);
		points
			.enter()
			.append('svg:circle')
			.attr('r', 2.5)
			.attr('class', 'point');
		points
			.attr('cx', function(d) { return xScale(d[0]) })
			.attr('cy', function(d) { return yScale1(d[2]) })
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
			.call(yAxis0);

		if (!dualYaxis)
			return;

		// Update the y1-axis.
		g.select(".y1.axis")
			.attr("transform", "translate(" + innerW() + ", 0)")
			.call(yAxis1);
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
				.tickSize(-innerW(), 0, 0)
				.tickFormat("")
			);
	}

	function formatTooltip(d, i) {
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
			.attr('x', function(d, i) { return (w * i); })
			.attr('y', 0)
			.on('mousemove', function(d, i) {
				mouseover(this, selection, d, i);
			})
			.on('mouseleave', function(d, i) {
				mouseout(selection, g);
			});
		// remove elements
		rects.exit().remove();
	}

	function chart(selection) {
		selection.each(function(data) {
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

	chart.labels = function(_) {
		if (!arguments.length) return labels;
		labels = _;
		return chart;
	};

	chart.margin = function(_) {
		if (!arguments.length) return margin;
		margin = _;
		return chart;
	};

	chart.width = function(_) {
		if (!arguments.length) return width;
		width = _;
		return chart;
	};

	chart.height = function(_) {
		if (!arguments.length) return height;
		height = _;
		return chart;
	};

	chart.x = function(_) {
		if (!arguments.length) return xValue;
		xValue = _;
		return chart;
	};

	chart.y0 = function(_) {
		if (!arguments.length) return yValue0;
		yValue0 = _;
		return chart;
	};

	chart.y1 = function(_) {
		if (!arguments.length) return yValue1;
		yValue1 = _;
		return chart;
	};

	return chart;
}

function BarChart(dualYaxis) {
	var margin = {
			top    : 10,
			right  : 50,
			bottom : 40,
			left   : 50
		},
		width = 760,
		height = 170,
		nTicks = 10;
	var labels = { x: 'Unnamed', y0: 'Unnamed', y1: 'Unnamed' };

	var	xValue = function(d) {
			return d[0];
		},
		yValue0 = function(d) {
			return d[1];
		},
		yValue1 = function(d) {
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
		.ticks(10, "s");

	var yAxis1 = d3.svg.axis()
		.scale(yScale1)
		.orient("right")
		.ticks(10, "s");

	var xGrid = d3.svg.axis()
		.scale(xScale)
		.orient("bottom");

	var yGrid = d3.svg.axis()
		.scale(yScale0)
		.orient("left")
		.ticks(10, "s");

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

		return d3.range(0, data.length, Math.round(data.length / nTicks)).map(function(d) {
			return xScale.domain()[d];
		});
	}

	// Convert data to standard representation greedily;
	// this is needed for nondeterministic accessors.
	function mapData(data) {
		var _datum = function(d, i) {
			var datum = [xValue.call(data, d, i), yValue0.call(data, d, i)];
			dualYaxis && datum.push(yValue1.call(data, d, i));
			return datum;
		};
		return data.map(function(d, i) {
			return _datum(d, i);
		});
	}

	function updateScales(data) {
		// Update the x-scale.
		xScale.domain(data.map(function(d) {
			return d[0];
		}))
		.rangeRoundBands([0, innerW()], .1);

		// Update the y-scale.
		yScale0.domain([0, d3.max(data, function(d) {
			return d[1];
		})])
		.range([innerH(), 0]);

		// Update the y-scale.
		dualYaxis && yScale1.domain([0, d3.max(data, function(d) {
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
			.attr("y", 6)
			.attr("x", -10)
			.attr("dy", ".75em")
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Labels
		var tEnter = svg.selectAll('text.axis-label.y1').data([null]);
		tEnter.enter().append("text")
			.attr("class", "axis-label y1")
			.attr("x", -10)
			.attr("dy", ".75em")
			.text(labels.y1);
		dualYaxis && tEnter
			.attr("y", width - 15)
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
			.attr('height', function(d) { return innerH() - yScale0(d[1]) })
			.attr('x', function(d) { return xScale(d[0]) })
			.attr('y', function(d) { return yScale0(d[1]) });
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
			.attr('height', function(d) { return innerH() - yScale1(d[2]) })
			.attr('x', function(d) { return (xScale(d[0]) + xScale.rangeBand() / 2) })
			.attr('y', function(d) { return yScale1(d[2]) });
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
			.call(yAxis0);

		if (!dualYaxis)
			return;

		// Update the y1-axis.
		g.select(".y1.axis")
			.attr("transform", "translate(" + innerW() + ", 0)")
			.call(yAxis1);
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
				.tickSize(-innerW(), 0, 0)
				.tickFormat("")
			);
	}

	function formatTooltip(d, i) {
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
			.attr('x', function(d, i) { return (w * i); })
			.attr('y', 0)
			.on('mousemove', function(d, i) {
				mouseover(this, selection, d, i);
			})
			.on('mouseleave', function(d, i) {
				mouseout(selection, g);
			});
		// remove elements
		rects.exit().remove();
	}

	function chart(selection) {
		selection.each(function(data) {
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

	chart.labels = function(_) {
		if (!arguments.length) return labels;
		labels = _;
		return chart;
	};

	chart.width = function(_) {
		if (!arguments.length) return width;
		width = _;
		return chart;
	};

	chart.height = function(_) {
		if (!arguments.length) return height;
		height = _;
		return chart;
	};

	chart.x = function(_) {
		if (!arguments.length) return xValue;
		xValue = _;
		return chart;
	};

	chart.y0 = function(_) {
		if (!arguments.length) return yValue0;
		yValue0 = _;
		return chart;
	};

	chart.y1 = function(_) {
		if (!arguments.length) return yValue1;
		yValue1 = _;
		return chart;
	};

	return chart;
}

// Init app
window.onload = function() {
	'use strict';

	GoAccess.start({
		'uidata': user_interface,
		'data': json_data
	});

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
