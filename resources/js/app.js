var GoAccess = (function() {
	'use strict';

	var AppCharts = {}, // holds all rendered charts
		AppUIData = {}, // holds panel definitions
		AppData   = {}, // hold raw data
		AppPrefs  = {
			'perPage': 11, // panel rows per page
		};

	// Helpers
	function $(selector) {
		return document.querySelector(selector);
	}

	function merge(o, n) {
		for (var attrname in n) {
			o[attrname] = n[attrname];
		}
	}

	function $$(selector, callback) {
		var elems = document.querySelectorAll(selector);
		for (var i = 0; i < elems.length; ++i) {
			if (callback && typeof callback == 'function')
				callback.call(this, elems[i]);
		}
	}

	function formatBytes(bytes, decimals) {
		if (bytes == 0) return '0 Byte';
		var k = 1024;
		var dm = decimals + 1 || 3;
		var sizes = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
		var i = Math.floor(Math.log(bytes) / Math.log(k));
		return (bytes / Math.pow(k, i)).toPrecision(dm) + ' ' + sizes[i];
	}

	function isNumeric(n) {
		return !isNaN(parseFloat(n)) && isFinite(n);
	}

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

	function getData(panel) {
		return panel ? AppData[panel] : AppData;
	}

	function getUIData(panel) {
		return panel ? AppUIData[panel] : AppUIData;
	}

	function renderGeneral() {
		var ui = getUIData('general');
		var data = getData('general');

		// General section wrapper
		var template = $('#tpl-general').innerHTML;
		$('.wrap-general').innerHTML = Hogan.compile(template).render(ui);

		var i = 0;
		var template = $('#tpl-general-items').innerHTML;
		var wrap = $('.wrap-general-items');

		for (var x in data) {
			if (!data.hasOwnProperty(x) || !ui.items.hasOwnProperty(x))
				continue;

			// create a new row bootstrap row every 6 elements
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

	function redrawChart(targ) {
		var plot = targ.getAttribute('data-plot');
		var panel = targ.getAttribute('data-panel');
		var ui = getUIData(panel);
		var plotUI = ui.plot;

		for (var x in plotUI) {
			if (!plotUI.hasOwnProperty(x) || plotUI[x].className != plot)
				continue;

			var chart = AppCharts[panel];

			var data = processChartData(getData(panel).data);
			if (ui.chartReverse)
				data = data.reverse();

			var p = plotUI[x].c3;
			console.log(p)
			p['data']['json'] = data;
			p['data']['unload'] = chart.json;

			chart.axis.labels(p.axis);
			chart.load(p.data);

			break;
		}
	}

	function setPanelExpanded(panel, targ) {
		var ui = getUIData(panel);
		if (targ.getAttribute('data-state') == 'collapsed')
			ui['expanded'] = true;
		else
			ui['expanded'] = false;
	}

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

	function renderPanel(x, ui) {
		var template = $('#tpl-panel').innerHTML;
		var box = document.createElement('div');
		box.id = 'panel-' + x;
		box.innerHTML = Hogan.compile(template).render(ui);
		$('.wrap-panels').appendChild(box);
	}

	function renderNav(nav) {
		var template = $('#tpl-panel-nav').innerHTML;
		$('.panel-nav').innerHTML = Hogan.compile(template).render({nav: nav});
	}

	function renderPanels() {
		var ui = getUIData();

		var nav = [];
		for (var x in ui) {
			if (!ui.hasOwnProperty(x) || !ui[x].id)
				continue;
			nav.push({
				'key': x,
				'head': ui[x].head,
			});
			renderPanel(x, ui[x]);
		}
		renderNav(nav);
		ePanelHandlers();
	}

	function c3Data(dataset, ui, type) {
		return {
			json: dataset,
			keys: ui.plot.length ? ui.plot[0].c3.data.keys : {},
			axes: ui.plot.length ? ui.plot[0].c3.data.axes : {},
			type: type,
		};
	}

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
		merge(def, ui.plot[0].c3.axis);

		return def;
	}

	function renderAreaSpline(panel, ui, dataset) {
		var chart = c3.generate({
			bindto: '#chart-' + panel,
			data: c3Data(dataset, ui, 'area-spline'),
			grid: c3Grid(),
			color: c3Palette(),
			axis: c3axis(panel),
			size: {
				height: 180
			}
		});
		AppCharts[panel] = chart;
	}

	function renderBar(panel, ui, dataset) {
		var chart = c3.generate({
			bindto: '#chart-' + panel,
			data: c3Data(dataset, ui, 'bar'),
			grid: c3Grid(),
			color: c3Palette(),
			axis: {
				x: {
					type: 'category',
					label: panel,
				},
				y: {
					label: 'Hits',
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

	function processChartData(data) {
		var out = [];
		for (var i = 0; i < data.length; ++i) {
			out.push(extractCount(data[i]));
		}
		return out;
	}

	function renderCharts() {
		var ui = getUIData();
		for (var panel in ui) {
			if (!ui.hasOwnProperty(panel))
				continue;
			if (!ui[panel].chartType || !ui[panel].plot.length)
				continue;

			var data = processChartData(getData(panel).data);
			if (ui[panel].chartReverse)
				data = data.reverse();

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

	function getCount(item) {
		if (typeof item == 'object' && 'count' in item)
			return item.count;
		return item;
	}

	function getPercent(item) {
		if (typeof item == 'object' && 'percent' in item)
			return fmtValue(item.percent, 'percent');
		return null;
	}

	function extractCount(item) {
		var o = {};
		for (var prop in item) {
			o[prop] = getCount(item[prop]);
		}
		return o;
	}

	function getTableCell(ui, value) {
		var className = ui.className || '';
		className += ui.valueType != 'string' ? 'text-right' : '';
		return {
			'className': className,
			'percent': getPercent(value),
			'value': fmtValue(getCount(value), ui.valueType)
		};
	}

	function getCells(panel, uiItem, dataItem) {
		// Iterate over the properties of each row and determine its type
		return getTableCell(uiItem, dataItem);
	}

	function iterUIItems(panel, uiItems, dataItems, callback) {
		var out = [];
		for (var i = 0; i < uiItems.length; ++i) {
			var uiItem = uiItems[i];
			var dataItem = dataItems[uiItem.key];
			if (callback && typeof callback == 'function') {
				var ret = callback.call(this, panel, uiItem, dataItem);
				if (ret) out.push(ret);
			}
		}
		return out;
	}

	function getCurPage(panel) {
		return getUIData(panel).curPage || 0;
	}

	function pageOffSet(panel) {
		var curPage = getUIData(panel).curPage || 0;
		return ((curPage - 1) * AppPrefs.perPage);
	}

	function getTotalPages(dataItems) {
		return Math.ceil(dataItems.length / AppPrefs.perPage)
	}

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

	function prevPage(panel) {
		var curPage = getUIData(panel).curPage || 0;
		return curPage - 1;
	}

	function nextPage(panel) {
		var curPage = getUIData(panel).curPage || 0;
		return curPage + 1;
	}

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
				'cells': iterUIItems(panel, uiItems, dataItems[i], getCells)
			});
		}

		return rows;
	}

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

	function getMetaValue(ui, value) {
		if ('meta' in ui)
			return value[ui.meta];
		return null;
	}

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

	function setUIOpts(panel, ui) {
		var dataItems = getData(panel).data;

		// expandable panel?
		ui['expandable'] = dataItems.length && dataItems[0].items ? true : false;
		// pagination
		ui['curPage'] = 0;
		ui['totalItems'] = dataItems.length;
	}

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

	function renderTable(panel, page, expanded) {
		var dataItems = getData(panel).data;
		var ui = getUIData(panel);
		if (ui.expanded) {
			dataItems = getAllDataItems(panel);
		}

		renderDataRows(panel, ui.items, dataItems, page);
	}

	function renderTables() {
		var ui = getUIData();

		for (var panel in ui) {
			if (!ui.hasOwnProperty(panel) || !ui[panel].id)
				continue;

			var uiItems = ui[panel].items;
			var data = getData(panel);
			if (data.hasOwnProperty('metadata'))
				renderMetaRow(panel, uiItems);
			if (data.hasOwnProperty('data'))
				renderDataRows(panel, uiItems, data.data, 0);
		}
	}

	function render() {
		renderGeneral();
		renderPanels();
		renderCharts();
		renderTables();
	}

	function initialize(options) {
		AppUIData = (options || {}).uidata;;
		AppData = (options || {}).data;;

		for (var panel in AppUIData) {
			if (!AppUIData.hasOwnProperty(panel) || !AppUIData[panel].id)
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
