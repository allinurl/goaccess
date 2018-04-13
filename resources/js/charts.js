/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2018 Gerardo Orellana <hello @ goaccess.io>
 */

'use strict';

// This is faster than calculating the exact length of each label.
// e.g., getComputedTextLength(), slice()...
function truncate(text, width) {
	text.each(function () {
		var parent = this.parentNode, $d3parent = d3.select(parent);
		var gw = $d3parent.node().getBBox();
		var x = (Math.min(gw.width, width) / 2) * -1;
		// adjust wrapper <svg> width
		if ('svg' == parent.nodeName)
			$d3parent.attr('width', width).attr('x', x);
		// wrap <text> within an svg
		else {
			$d3parent.insert('svg', function () {
				return this;
			}.bind(this))
			.attr('class', 'wrap-text')
			.attr('width', width)
			.attr('x', x)
			.append(function () {
				return this;
			}.bind(this));
		}
	});
}

function AreaChart(dualYaxis) {
	var opts = {};
	var margin = {
			top    : 20,
			right  : 50,
			bottom : 40,
			left   : 50,
		},
		height = 170,
		nTicks = 10,
		padding = 10,
		width = 760;
	var labels = { x: 'Unnamed', y0: 'Unnamed', y1: 'Unnamed' };
	var format = { x: null, y0: null, y1: null};

	var xValue = function (d) {
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
		.orient('bottom')
		.tickFormat(function (d) {
			if (format.x)
				return GoAccess.Util.fmtValue(d, format.x);
			return d;
		});

	var yAxis0 = d3.svg.axis()
		.scale(yScale0)
		.orient('left')
		.tickFormat(function (d) {
			return d3.format('.2s')(d);
		});

	var yAxis1 = d3.svg.axis()
		.scale(yScale1)
		.orient('right')
		.tickFormat(function (d) {
			if (format.y1)
				return GoAccess.Util.fmtValue(d, format.y1);
			return d3.format('.2s')(d);
		});

	var xGrid = d3.svg.axis()
		.scale(xScale)
		.orient('bottom');

	var yGrid = d3.svg.axis()
		.scale(yScale0)
		.orient('left');

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

		return d3.range(0, data.length, Math.ceil(data.length / nTicks)).map(function (d) {
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

	function toggleOpacity(ele, op) {
		d3.select(ele.parentNode).selectAll('.' + (ele.getAttribute('data-yaxis') == 'y0' ? 'y1' : 'y0')).attr('style', op);
	}

	function setLegendLabels(svg) {
		// Legend Color
		var rect = svg.selectAll('rect.legend.y0').data([null]);
		rect.enter().append('rect')
			.attr('class', 'legend y0')
			.attr('data-yaxis', 'y0')
			.on('mousemove', function (d, i) { toggleOpacity(this, 'opacity:0.1'); })
			.on('mouseleave', function (d, i) { toggleOpacity(this, null); })
			.attr('y', (height - 15));
		rect
			.attr('x', (width / 2) - 100);

		// Legend Labels
		var text = svg.selectAll('text.legend.y0').data([null]);
		text.enter().append('text')
			.attr('class', 'legend y0')
			.attr('data-yaxis', 'y0')
			.on('mousemove', function (d, i) { toggleOpacity(this, 'opacity:0.1'); })
			.on('mouseleave', function (d, i) { toggleOpacity(this, null); })
			.attr('y', (height - 6));
		text
			.attr('x', (width / 2) - 85)
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Legend Labels
		rect = svg.selectAll('rect.legend.y1').data([null]);
		rect.enter().append('rect')
			.attr('class', 'legend y1')
			.attr('data-yaxis', 'y1')
			.on('mousemove', function (d, i) { toggleOpacity(this, 'opacity:0.1'); })
			.on('mouseleave', function (d, i) { toggleOpacity(this, null); })
			.attr('y', (height - 15));
		rect
			.attr('x', (width / 2));

		// Legend Labels
		text = svg.selectAll('text.legend.y1').data([null]);
		text.enter().append('text')
			.attr('class', 'legend y1')
			.attr('data-yaxis', 'y1')
			.on('mousemove', function (d, i) { toggleOpacity(this, 'opacity:0.1'); })
			.on('mouseleave', function (d, i) { toggleOpacity(this, null); })
			.attr('y', (height - 6));
		text
			.attr('x', (width / 2) + 15)
			.text(labels.y1);
	}

	function setAxisLabels(svg) {
		// Labels
		svg.selectAll('text.axis-label.y0').data([null])
			.enter().append('text')
			.attr('class', 'axis-label y0')
			.attr('y', 10)
			.attr('x', 53)
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Labels
		var tEnter = svg.selectAll('text.axis-label.y1').data([null]);
		tEnter.enter().append('text')
			.attr('class', 'axis-label y1')
			.attr('y', 10)
			.text(labels.y1);
		dualYaxis && tEnter
			.attr('x', width - 25);
	}

	function createSkeleton(svg) {
		// Otherwise, create the skeletal chart.
		var gEnter = svg.enter().append('svg').append('g');

		// Lines
		gEnter.append('g')
			.attr('class', 'line line0 y0');
		dualYaxis && gEnter.append('g')
			.attr('class', 'line line1 y1');

		// Areas
		gEnter.append('g')
			.attr('class', 'area area0 y0');
		dualYaxis && gEnter.append('g')
			.attr('class', 'area area1 y1');

		// Points
		gEnter.append('g')
			.attr('class', 'points y0');
		dualYaxis && gEnter.append('g')
			.attr('class', 'points y1');

		// Grid
		gEnter.append('g')
			.attr('class', 'x grid');
		gEnter.append('g')
			.attr('class', 'y grid');

		// Axis
		gEnter.append('g')
			.attr('class', 'x axis');
		gEnter.append('g')
			.attr('class', 'y0 axis');
		dualYaxis && gEnter.append('g')
			.attr('class', 'y1 axis');

		// Rects
		gEnter.append('g')
			.attr('class', 'rects');

		setAxisLabels(svg);
		setLegendLabels(svg);

		// Mouseover line
		gEnter.append('line')
			.attr('y2', innerH())
			.attr('y1', 0)
			.attr('class', 'indicator');
	}

	function pathLen(d) {
		return d.node().getTotalLength();
	}

	function addLine(g, data, line, cName) {
		// Update the line path.
		var path = g.select('g.' + cName).selectAll('path.' + cName)
			.data([data]);
		// enter
		path
			.enter()
			.append('svg:path')
			.attr('d', line)
			.attr('class', cName)
			.attr('stroke-dasharray', function (d) {
				var pl = pathLen(d3.select(this));
				return pl + ' ' + pl;
			})
			.attr('stroke-dashoffset', function (d) {
				return pathLen(d3.select(this));
			});
		// update
		path
			.attr('d', line)
			.transition()
			.attr('stroke-dasharray', function (d) {
				var pl = pathLen(d3.select(this));
				return pl + ' ' + pl;
			})
			.duration(2000)
			.attr('stroke-dashoffset', 0);
		// remove elements
		path.exit().remove();
	}

	function addArea(g, data, cb, cName) {
		// Update the area path.
		var area = g.select('g.' + cName).selectAll('path.' + cName)
			.data([data]);
		area
			.enter()
			.append('svg:path')
			.attr('class', cName);
		area
			.attr('d', cb);
		// remove elements
		area.exit().remove();
	}

	// Update the area path and lines.
	function addAreaLines(g, data) {
		// Update the area path.
		addArea(g, data, area0.y0(yScale0.range()[0]), 'area0');
		// Update the line path.
		addLine(g, data, line0, 'line0');
		// Update the area path.
		addArea(g, data, area1.y1(yScale1.range()[0]), 'area1');
		// Update the line path.
		addLine(g, data, line1, 'line1');
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
			.attr('cx', function (d) { return xScale(d[0]); })
			.attr('cy', function (d) { return yScale0(d[1]); });
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
			.attr('cx', function (d) { return xScale(d[0]); })
			.attr('cy', function (d) { return yScale1(d[2]); });
		// remove elements
		points.exit().remove();
	}

	function addAxis(g, data) {
		var xTicks = getXTicks(data);
		var tickDistance = xTicks.length > 1 ? (xScale(xTicks[1]) - xScale(xTicks[0])) : innerW();
		var labelW = tickDistance - padding;

		// Update the x-axis.
		g.select('.x.axis')
			.attr('transform', 'translate(0,' + yScale0.range()[0] + ')')
			.call(xAxis.tickValues(xTicks))
			.selectAll(".tick text")
			.call(truncate, labelW > 0 ? labelW : innerW());

		// Update the y0-axis.
		g.select('.y0.axis')
			.call(yAxis0.tickValues(getYTicks(yScale0)));

		if (!dualYaxis)
			return;

		// Update the y1-axis.
		g.select('.y1.axis')
			.attr('transform', 'translate(' + innerW() + ', 0)')
			.call(yAxis1.tickValues(getYTicks(yScale1)));
	}

	// Update the X-Y grid.
	function addGrid(g, data) {
		g.select('.x.grid')
			.attr('transform', 'translate(0,' + yScale0.range()[0] + ')')
			.call(xGrid
				.tickValues(getXTicks(data))
				.tickSize(-innerH(), 0, 0)
				.tickFormat('')
			);

		g.select('.y.grid')
			.call(yGrid
				.tickValues(getYTicks(yScale0))
				.tickSize(-innerW(), 0, 0)
				.tickFormat('')
			);
	}

	function formatTooltip(data, i) {
		var d = data.slice(0);

		d[0] = (format.x) ? GoAccess.Util.fmtValue(d[0], format.x) : d[0];
		d[1] = (format.y0) ? GoAccess.Util.fmtValue(d[1], format.y0) : d3.format(',')(d[1]);
		dualYaxis && (d[2] = (format.y1) ? GoAccess.Util.fmtValue(d[2], format.y1) : d3.format(',')(d[2]));

		var template = d3.select('#tpl-chart-tooltip').html();
		return Hogan.compile(template).render({
			'data': d
		});
	}

	function mouseover(_self, selection, data, idx) {
		var tooltip = selection.select('.chart-tooltip-wrap');
		tooltip.html(formatTooltip(data, idx))
			.style('left', (xScale(data[0])) + 'px')
			.style('top',  (d3.mouse(_self)[1] + 10) + 'px')
			.style('display', 'block');

		selection.select('line.indicator')
			.style('display', 'block')
			.attr('transform', 'translate(' + xScale(data[0]) + ',' + 0 + ')');
	}

	function mouseout(selection, g) {
		var tooltip = selection.select('.chart-tooltip-wrap');
		tooltip.style('display', 'none');

		g.select('line.indicator').style('display', 'none');
	}

	function addRects(selection, g, data) {
		var w = (innerW() / data.length);

		var rects = g.select('g.rects').selectAll('rect')
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
			var svg = d3.select(this).selectAll('svg').data([data]);
			createSkeleton(svg);

			// Update the outer dimensions.
			svg.attr({
				'width': width,
				'height': height
			});

			// Update the inner dimensions.
			var g = svg.select('g')
				.attr('transform', 'translate(' + margin.left + ',' + margin.top + ')');

			// Add grid
			addGrid(g, data);
			// Add chart lines and areas
			addAreaLines(g, data);
			// Add chart points
			addPoints(g, data);
			// Add axis
			addAxis(g, data);
			// Add rects
			addRects(selection, g, data);
		});
	}

	chart.opts = function (_) {
		if (!arguments.length) return opts;
		opts = _;
		return chart;
	};

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
	var opts = {};
	var margin = {
			top    : 20,
			right  : 50,
			bottom : 40,
			left   : 50,
		},
		height = 170,
		nTicks = 10,
		padding = 10,
		width = 760;
	var labels = { x: 'Unnamed', y0: 'Unnamed', y1: 'Unnamed' };
	var format = { x: null, y0: null, y1: null};

	var xValue = function (d) {
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
		.orient('bottom')
		.tickFormat(function (d) {
			if (format.x)
				return GoAccess.Util.fmtValue(d, format.x);
			return d;
		});

	var yAxis0 = d3.svg.axis()
		.scale(yScale0)
		.orient('left')
		.tickFormat(function (d) {
			return d3.format('.2s')(d);
		});

	var yAxis1 = d3.svg.axis()
		.scale(yScale1)
		.orient('right')
		.tickFormat(function (d) {
			if (format.y1)
				return GoAccess.Util.fmtValue(d, format.y1);
			return d3.format('.2s')(d);
		});

	var xGrid = d3.svg.axis()
		.scale(xScale)
		.orient('bottom');

	var yGrid = d3.svg.axis()
		.scale(yScale0)
		.orient('left');

	function innerW() {
		return width - margin.left - margin.right;
	}

	function innerH() {
		return height - margin.top - margin.bottom;
	}

	function getXTicks(data) {
		if (data.length < nTicks)
			return xScale.domain();

		return d3.range(0, data.length, Math.ceil(data.length / nTicks)).map(function (d) {
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
		.rangeBands([0, innerW()], .1);

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

	function toggleOpacity(ele, op) {
		d3.select(ele.parentNode).selectAll('.' + (ele.getAttribute('data-yaxis') == 'y0' ? 'y1' : 'y0')).attr('style', op);
	}

	function setLegendLabels(svg) {
		// Legend Color
		var rect = svg.selectAll('rect.legend.y0').data([null]);
		rect.enter().append('rect')
			.attr('class', 'legend y0')
			.attr('data-yaxis', 'y0')
			.on('mousemove', function (d, i) { toggleOpacity(this, 'opacity:0.1'); })
			.on('mouseleave', function (d, i) { toggleOpacity(this, null); })
			.attr('y', (height - 15));
		rect
			.attr('x', (width / 2) - 100);

		// Legend Labels
		var text = svg.selectAll('text.legend.y0').data([null]);
		text.enter().append('text')
			.attr('class', 'legend y0')
			.attr('data-yaxis', 'y0')
			.on('mousemove', function (d, i) { toggleOpacity(this, 'opacity:0.1'); })
			.on('mouseleave', function (d, i) { toggleOpacity(this, null); })
			.attr('y', (height - 6));
		text
			.attr('x', (width / 2) - 85)
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Legend Labels
		rect = svg.selectAll('rect.legend.y1').data([null]);
		rect.enter().append('rect')
			.attr('class', 'legend y1')
			.attr('data-yaxis', 'y1')
			.on('mousemove', function (d, i) { toggleOpacity(this, 'opacity:0.1'); })
			.on('mouseleave', function (d, i) { toggleOpacity(this, null); })
			.attr('y', (height - 15));
		rect
			.attr('x', (width / 2));

		// Legend Labels
		text = svg.selectAll('text.legend.y1').data([null]);
		text.enter().append('text')
			.attr('class', 'legend y1')
			.attr('data-yaxis', 'y1')
			.on('mousemove', function (d, i) { toggleOpacity(this, 'opacity:0.1'); })
			.on('mouseleave', function (d, i) { toggleOpacity(this, null); })
			.attr('y', (height - 6));
		text
			.attr('x', (width / 2) + 15)
			.text(labels.y1);
	}

	function setAxisLabels(svg) {
		// Labels
		svg.selectAll('text.axis-label.y0').data([null])
			.enter().append('text')
			.attr('class', 'axis-label y0')
			.attr('y', 10)
			.attr('x', 53)
			.text(labels.y0);

		if (!dualYaxis)
			return;

		// Labels
		var tEnter = svg.selectAll('text.axis-label.y1').data([null]);
		tEnter.enter().append('text')
			.attr('class', 'axis-label y1')
			.attr('y', 10)
			.text(labels.y1);
		dualYaxis && tEnter
			.attr('x', width - 25);
	}

	function createSkeleton(svg) {
		// Otherwise, create the skeletal chart.
		var gEnter = svg.enter().append('svg').append('g');

		// Grid
		gEnter.append('g')
			.attr('class', 'x grid');
		gEnter.append('g')
			.attr('class', 'y grid');

		// Axis
		gEnter.append('g')
			.attr('class', 'x axis');
		gEnter.append('g')
			.attr('class', 'y0 axis');
		dualYaxis && gEnter.append('g')
			.attr('class', 'y1 axis');

		// Bars
		gEnter.append('g')
			.attr('class', 'bars y0');
		dualYaxis && gEnter.append('g')
			.attr('class', 'bars y1');

		// Rects
		gEnter.append('g')
			.attr('class', 'rects');

		setAxisLabels(svg);
		setLegendLabels(svg);

		// Mouseover line
		gEnter.append('line')
			.attr('y2', innerH())
			.attr('y1', 0)
			.attr('class', 'indicator');
	}

	// Update the area path and lines.
	function addBars(g, data) {
		var bars = g.select('g.bars.y0').selectAll('rect.bar')
			.data(data);
		// enter
		bars
			.enter()
			.append('svg:rect')
			.attr('class', 'bar')
			.attr('height', 0)
			.attr('width', function (d, i) { return xScale.rangeBand() / 2; })
			.attr('x', function (d, i) { return xScale(d[0]); })
			.attr('y', function (d, i) { return innerH(); });
		// update
		bars
			.attr('width', xScale.rangeBand() / 2)
			.attr('x', function (d) { return xScale(d[0]); })
			.transition()
			.delay(function (d, i) { return i / data.length * 1000; })
			.duration(500)
			.attr('height', function (d, i) { return innerH() - yScale0(d[1]); })
			.attr('y', function (d, i) { return yScale0(d[1]); });
		// remove elements
		bars.exit().remove();

		if (!dualYaxis)
			return;

		var bars = g.select('g.bars.y1').selectAll('rect.bar')
			.data(data);
		// enter
		bars
			.enter()
			.append('svg:rect')
			.attr('class', 'bar')
			.attr('height', 0)
			.attr('width', function (d, i) { return xScale.rangeBand() / 2; })
			.attr('x', function (d) { return (xScale(d[0]) + xScale.rangeBand() / 2); })
			.attr('y', function (d, i) { return innerH(); });
		// update
		bars
			.attr('width', xScale.rangeBand() / 2)
			.attr('x', function (d) { return (xScale(d[0]) + xScale.rangeBand() / 2); })
			.transition()
			.delay(function (d, i) { return i / data.length * 1000; })
			.duration(500)
			.attr('height', function (d, i) { return innerH() - yScale1(d[2]); })
			.attr('y', function (d, i) { return yScale1(d[2]); });
		// remove elements
		bars.exit().remove();
	}

	function addAxis(g, data) {
		var xTicks = getXTicks(data);
		var tickDistance = xTicks.length > 1 ? (xScale(xTicks[1]) - xScale(xTicks[0])) : innerW();
		var labelW = tickDistance - padding;

		// Update the x-axis.
		g.select('.x.axis')
			.attr('transform', 'translate(0,' + yScale0.range()[0] + ')')
			.call(xAxis.tickValues(xTicks))
			.selectAll(".tick text")
			.call(truncate, labelW > 0 ? labelW : innerW());

		// Update the y0-axis.
		g.select('.y0.axis')
			.call(yAxis0.tickValues(getYTicks(yScale0)));

		if (!dualYaxis)
			return;

		// Update the y1-axis.
		g.select('.y1.axis')
			.attr('transform', 'translate(' + innerW() + ', 0)')
			.call(yAxis1.tickValues(getYTicks(yScale1)));
	}

	// Update the X-Y grid.
	function addGrid(g, data) {
		g.select('.x.grid')
			.attr('transform', 'translate(0,' + yScale0.range()[0] + ')')
			.call(xGrid
				.tickValues(getXTicks(data))
				.tickSize(-innerH(), 0, 0)
				.tickFormat('')
			);

		g.select('.y.grid')
			.call(yGrid
				.tickValues(getYTicks(yScale0))
				.tickSize(-innerW(), 0, 0)
				.tickFormat('')
			);
	}

	function formatTooltip(data, i) {
		var d = data.slice(0);

		d[0] = (format.x) ? GoAccess.Util.fmtValue(d[0], format.x) : d[0];
		d[1] = (format.y0) ? GoAccess.Util.fmtValue(d[1], format.y0) : d3.format(',')(d[1]);
		dualYaxis && (d[2] = (format.y1) ? GoAccess.Util.fmtValue(d[2], format.y1) : d3.format(',')(d[2]));

		var template = d3.select('#tpl-chart-tooltip').html();
		return Hogan.compile(template).render({
			'data': d
		});
	}

	function mouseover(_self, selection, data, idx) {
		var left = xScale(data[0]) + (xScale.rangeBand() / 2);
		var tooltip = selection.select('.chart-tooltip-wrap');
		tooltip.html(formatTooltip(data, idx))
			.style('left', left + 'px')
			.style('top',  (d3.mouse(_self)[1] + 10) + 'px')
			.style('display', 'block');

		selection.select('line.indicator')
			.style('display', 'block')
			.attr('transform', 'translate(' + left + ',' + 0 + ')');
	}

	function mouseout(selection, g) {
		var tooltip = selection.select('.chart-tooltip-wrap');
		tooltip.style('display', 'none');

		g.select('line.indicator').style('display', 'none');
	}

	function addRects(selection, g, data) {
		var w = (innerW() / data.length);

		var rects = g.select('g.rects').selectAll('rect')
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
			var svg = d3.select(this).selectAll('svg').data([data]);
			createSkeleton(svg);

			// Update the outer dimensions.
			svg.attr({
				'width': width,
				'height': height
			});

			// Update the inner dimensions.
			var g = svg.select('g')
				.attr('transform', 'translate(' + margin.left + ',' + margin.top + ')');

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

	chart.opts = function (_) {
		if (!arguments.length) return opts;
		opts = _;
		return chart;
	};

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
