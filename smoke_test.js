const http = require('http');

const base = 'http://localhost:3000';

function request(method, path, body) {
  return new Promise((resolve, reject) => {
    const url = new URL(path, base);
    const options = {
      method,
      hostname: url.hostname,
      port: url.port,
      path: url.pathname,
      headers: body ? { 'Content-Type': 'application/json' } : {}
    };
    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', (chunk) => data += chunk);
      res.on('end', () => resolve({ status: res.statusCode, body: data }));
    });
    req.on('error', reject);
    if (body) req.write(JSON.stringify(body));
    req.end();
  });
}

async function main() {
  // 1. POST a kwh value
  console.log('POST /consumer-purchase with { kwh: 12.5 }');
  const postRes = await request('POST', '/consumer-purchase', { kwh: 12.5 });
  console.log('Response:', postRes.status, postRes.body);

  // 2. GET the stored value (should also clear it)
  console.log('\nGET /consumer-purchase');
  const getRes = await request('GET', '/consumer-purchase');
  console.log('Response:', getRes.status, getRes.body);

  // 3. GET again - should be 404 since it was cleared
  console.log('\nGET /consumer-purchase again (expect 404)');
  const getRes2 = await request('GET', '/consumer-purchase');
  console.log('Response:', getRes2.status, getRes2.body);

  // 4. POST with missing kwh
  console.log('\nPOST /consumer-purchase with empty body (expect 400)');
  const postResBad = await request('POST', '/consumer-purchase', {});
  console.log('Response:', postResBad.status, postResBad.body);

  // 5. POST with negative kwh
  console.log('\nPOST /consumer-purchase with negative kwh (expect 400)');
  const postResNeg = await request('POST', '/consumer-purchase', { kwh: -5 });
  console.log('Response:', postResNeg.status, postResNeg.body);
}

main().catch(console.error);
