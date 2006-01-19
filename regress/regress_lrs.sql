-- No M value
select '2d',asewkt(locate_along_measure('POINT(1 2)', 1));
select '3dz',asewkt(locate_along_measure('POINT(1 2 3)', 1));

-- Points
select 'PNTM_1',asewkt(locate_along_measure('POINTM(1 2 3)', 1));
select 'PNTM_2',asewkt(locate_along_measure('POINTM(1 2 3)', 3));
select 'PNTM_3',asewkt(locate_between_measures('POINTM(1 2 3)', 2, 3));
select 'PNTM_4',asewkt(locate_between_measures('POINTM(1 2 3)', 3, 4));
select 'PNTM_5',asewkt(locate_between_measures('POINTM(1 2 4.00001)', 3, 4));

-- Multipoints
select 'MPNT_1',asewkt(locate_between_measures('MULTIPOINTM(1 2 2)', 2, 5));
select 'MPNT_2', asewkt(locate_between_measures('MULTIPOINTM(1 2 8, 2 2 5, 2 1 0)', 2, 5));
select 'MPNT_3', asewkt(locate_between_measures('MULTIPOINTM(1 2 8, 2 2 5.1, 2 1 0)', 2, 5));
select 'MPNT_4', asewkt(locate_between_measures('MULTIPOINTM(1 2 8, 2 2 5, 2 1 0)', 4, 8));

-- Linestrings
select 'LINEZM_1', asewkt(locate_between_measures('LINESTRING(0 10 100 0, 0 0 0 10)', 2, 18));
select 'LINEZM_2', asewkt(locate_between_measures('LINESTRING(0 10 0 0, 0 0 100 10)', 2, 18));
select 'LINEZM_3', asewkt(locate_between_measures('LINESTRING(0 10 100 0, 0 0 0 10, 10 0 0 0)', 2, 18));
select 'LINEZM_4', asewkt(locate_between_measures('LINESTRING(0 10 100 0, 0 0 0 20, 10 0 0 0)', 2, 18));
select 'LINEZM_5', asewkt(locate_between_measures('LINESTRING(0 10 100 0, 0 0 0 20, 0 10 10 40, 10 0 0 0)', 2, 18));
select 'LINEZM_6', asewkt(locate_between_measures('LINESTRING(0 10 10 40, 10 0 0 0)', 2, 2));

--- line_locate_point

SELECT 'line_locate_point', line_locate_point('LINESTRING(709243.393033887 163969.752725768,708943.240904444 163974.593889146,708675.634380651 163981.832927298)', 'POINT(705780 15883)');

--- postgis-users/2006-January/010613.html
select 'line_locate_point', line_locate_point(geomfromtext('LINESTRING(-1953743.873 471070.784,-1953735.105 471075.419,-1953720.034 471081.649)', 6269), geomfromtext('POINT(-1953720.034 471081.649)', 6269));
select 'line_locate_point', line_locate_point(geomfromtext('LINESTRING(-1953743.873 471070.784,-1953735.105 471075.419,-1953720.034 471081.649)', 6269), geomfromtext('POINT(-1953743.873 471070.784)', 6269));

--- postgis-devel/2006-January/001951.html
select 'line_substring', asewkt(line_substring(geomfromewkt('SRID=4326;LINESTRING(0 0 0 0, 1 1 1 1, 2 2 2 2, 3 3 3 3, 4 4 4 4)'), 0.5, 0.8));

select 'line_substring', asewkt(line_substring('LINESTRING(0 0 0 0, 1 1 1 1, 2 2 2 2, 3 3 3 3, 4 4 4 4)', 0.5, 0.75));
select 'line_substring', asewkt(line_substring('LINESTRING(0 0, 1 1, 2 2)', 0, 0.5));
select 'line_substring', asewkt(line_substring('LINESTRING(0 0, 1 1, 2 2)', 0.5, 1));
select 'line_substring', asewkt(line_substring('LINESTRING(0 0, 2 2)', 0.5, 1));
select 'line_substring', asewkt(line_substring('LINESTRING(0 0, 2 2)', 0, 0.5));
select 'line_substring', asewkt(line_substring('LINESTRING(0 0, 4 4)', .25, 0.5));
select 'line_substring', asewkt(line_substring('LINESTRINGM(0 0 0, 4 4 4)', .25, 0.5));
select 'line_substring', asewkt(line_substring('LINESTRINGM(0 0 4, 4 4 0)', .25, 0.5));
select 'line_substring', asewkt(line_substring('LINESTRING(0 0 4, 4 4 0)', .25, 0.5));

select 'line_substring', asewkt(line_substring('LINESTRING(0 0, 1 1)', 0, 0));
select 'line_substring', asewkt(line_substring('LINESTRING(0 0 10, 1 1 5)', 0.5, .5));
