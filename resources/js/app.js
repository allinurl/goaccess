var GoAccess = (function() {
	'use strict';

	var AppCharts = {}, // holds all rendered charts
		AppUIData = {}, // holds panel definitions
		AppData   = {}, // hold raw data
		AppPrefs  = {
			'perPage': 11, // panel rows per page
		};

	// Helpers
	// Syntactic sugar
	function $(selector) {
		return document.querySelector(selector);
	}

	// Add all attributes of n to o
	function merge(o, n) {
		for (var attrname in n) {
			o[attrname] = n[attrname];
		}
	}

	// Syntactic sugar & execute callback
	function $$(selector, callback) {
		var elems = document.querySelectorAll(selector);
		for (var i = 0; i < elems.length; ++i) {
			if (callback && typeof callback == 'function')
				callback.call(this, elems[i]);
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

			var chart = AppCharts[panel];

			// Extract data for the selected panel and process it
			var data = processChartData(getData(panel).data);
			if (ui.chartReverse)
				data = data.reverse();

			var p = plotUI[x].c3;
			p['data']['json'] = data;
			p['data']['unload'] = chart.json;

			chart.axis.labels(p.axis);
			chart.load(p.data);

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
		$('.panel-nav').innerHTML = Hogan.compile(template).render({nav: nav});
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

	// Set C3 data to plot
	function c3Data(dataset, ui, type) {
		return {
			json: dataset,
			keys: ui.plot.length ? ui.plot[0].c3.data.keys : {},
			axes: ui.plot.length ? ui.plot[0].c3.data.axes : {},
			type: type,
		};
	}

	// Set a default C3 palette
	function c3Palette() {
		return {
			'pattern': [
				'#447FB3',
				'#FF6854',
				'#42C966',
				'#FFB854',
				'#E29A36',
				'#FF8373',
				'#69A3D7',
				'#28A94A'
			]
		};
	}

	// Set default C3 grid lines. Display X and Y axis by default.
	function c3Grid() {
		return {
			'x': {
				'show': true
			},
			'y': {
				'show': true
			},
		};
	}

	// Set default C3 X axis and set a default number of max elements.
	function c3axis(panel) {
		var ui = getUIData(panel);
		var def = {
			x: {
				type: 'category',
				label: panel,
				tick: {
					culling: {
						max: 10
					}
				}
			}
		};
		// Merge object with panel c3 axis definition.
		merge(def, ui.plot[0].c3.axis);

		// TODO: it's dirty to overwrite the formater
		def.y.tick.format = d3.format("s");

		return def;
	}

	// Set default C3 tooltip. It also applies formatting given the data type.
	function c3Tooltip() {
		var tooltip = {
			format: {
				value: function (value, ratio, id) {
					switch (id) {
					case 'bytes':
						return fmtValue(value, 'bytes');
					case 'visitors':
					case 'hits':
						return d3.format('s')(value);
					default:
						return value;
					}
				},
				name: function (name, ratio, id, index) {
					switch (id) {
					case 'bytes':
						return 'Traffic';
					default:
						return name;
					}
				}
			}
		};

		return tooltip;
	}

	// Set default C3 chart to area spline and apply panel user interface
	// definition and load data.
	function renderAreaSpline(panel, ui, dataset) {
		var chart = c3.generate({
			bindto: '#chart-' + panel,
			data: c3Data(dataset, ui, 'area-spline'),
			grid: c3Grid(),
			color: c3Palette(),
			axis: c3axis(panel),
			tooltip: c3Tooltip(),
			size: {
				height: 180
			}
		});
		AppCharts[panel] = chart;
	}

	// Set default C3 chart to bar and apply panel user interface definition
	// and load data.
	function renderBar(panel, ui, dataset) {
		var chart = c3.generate({
			bindto: '#chart-' + panel,
			data: c3Data(dataset, ui, 'bar'),
			grid: c3Grid(),
			color: c3Palette(),
			tooltip: c3Tooltip(),
			axis: {
				x: {
					type: 'category',
					label: panel,
				},
				y: {
					label: 'Hits',
					tick: {
						format: d3.format('s')
					}
				},
				y2: {
					show: true,
					label: 'Visitors',
				}
			},
			size: {
				height: 180
			}
		});
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
				renderAreaSpline(panel, ui[panel], data);
				break;
			case 'bar':
				renderBar(panel, ui[panel], data);
				break;
			};
		}
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

		// Iterate over all data items within the given panel
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
			panelHtml.getElementsByClassName('panel-next')[0].parentNode.className  = '';
			panelHtml.getElementsByClassName('panel-prev')[0].parentNode.className  = '';

			// Diable pagination next button if last page is reached
			if (page >= getTotalPages(dataItems))
				panelHtml.getElementsByClassName('panel-next')[0].parentNode.className  = 'disabled';

			// Disable pagination prev button if first page is reached
			if (page <= 1)
				panelHtml.getElementsByClassName('panel-prev')[0].parentNode.className  = 'disabled';
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

// Init app
window.onload = function () {
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
