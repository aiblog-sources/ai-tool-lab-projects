# orders-api

Small Express service that stores orders in memory and asks a tax service for the rate.

    npm install
    JWT_SECRET=dev TAX_SERVICE_URL=http://localhost:4000 npm start
