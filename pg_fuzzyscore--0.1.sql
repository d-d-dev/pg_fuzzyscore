\echo Use "CREATE EXTENSION pg_fuzzyscore" to load this file. \quit

CREATE OR REPLACE FUNCTION fuzzyprepare(input text) RETURNS bytea
AS 'MODULE_PATHNAME', 'fuzzyprepare'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION fuzzyscore(prepared bytea, search text) RETURNS float
AS 'MODULE_PATHNAME', 'fuzzyscore'
LANGUAGE C IMMUTABLE STRICT;