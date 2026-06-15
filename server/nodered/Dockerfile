# Node-RED + palettes nécessaires au flow (dashboard, MongoDB, InfluxDB).
FROM nodered/node-red:4.0

RUN npm install --no-audit --no-fund \
      node-red-dashboard \
      node-red-node-mongodb \
      node-red-contrib-influxdb

# Flow intégré à l'image (pas de bind-mount fichier unique -> évite l'EBUSY
# Windows au moment du Deploy). Au 1er démarrage, Docker recopie /data dans le
# volume nommé vide ; ensuite les Deploy persistent dans le volume.
COPY --chown=node-red:node-red flows.json /data/flows.json
