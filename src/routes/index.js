const express = require('express');
const healthRouter = require('./health');
const buttonRouter = require('./button');

const router = express.Router();

router.use('/health', healthRouter);
router.use('/button', buttonRouter);

// Root endpoint
router.get('/', (req, res) => {
  res.json({ message: 'Welcome to the API' });
});

module.exports = router;
