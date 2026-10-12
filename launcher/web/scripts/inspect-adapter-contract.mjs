#!/usr/bin/env node
import { PRODUCT_GAMES } from '../lib/contracts/product-catalog.mjs';
import { createAdapterContractReport } from '../lib/adapter-contract-report.mjs';

const args = process.argv.slice(2);
if (args.length > 1 || (args.length && !/^--game=th[0-9]{2}$/.test(args[0])))
  throw new Error('用法：node scripts/inspect-adapter-contract.mjs [--game=thNN]');
const game = args[0]?.slice(7);
const reports = (game ? [game] : Object.keys(PRODUCT_GAMES)).map(createAdapterContractReport);
console.log(JSON.stringify(game ? reports[0] : reports, null, 2));
