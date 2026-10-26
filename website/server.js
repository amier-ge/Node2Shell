'use strict';

const express = require('express');
const service = express();

service.get('/health', (_req, res) => res.end('PONG\n'));
service.post('/upload', express.json(), (_req, res) => res.end('DONE\n'));
service.use((error, _req, res, _next) => {
  res.status(400).end(`ERR ${error.code || error.type || error.message}\n`);
});
service.listen(3000, '0.0.0.0');
