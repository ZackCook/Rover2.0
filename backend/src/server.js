import express from "express";
import { WebSocketServer } from "ws";
import { v4 as uuidv4 } from "uuid";
import http from "http";
import path from "path";
import { fileURLToPath } from "url";
import { log } from "console";

// --- This block fixes __dirname for ES Modules ---
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
// -------------------------------------------------

// --- Configuration ---
const PORT = 3000;
const HOST = "0.0.0.0"; // Listen on all network interfaces
const SERVER_ID = "server-main"; // The server's unique ID

/**
 * Represents a connected client (either a Rover or a UI).
 * This class encapsulates all the information and state related to a single WebSocket connection.
 */
class Client {
  #ws;
  #id;
  #type;

  constructor(ws, id) {
    this.#ws = ws;
    this.#id = id;
    this.#type = "unknown"; // All clients start as 'unknown' until they verify.
  }

  get id() {
    return this.#id;
  }
  get ws() {
    return this.#ws;
  }
  get type() {
    return this.#type;
  }

  set type(newType) {
    this.#type = newType;
  }

  /**
   * Attempts to verify and "graduate" the client.
   * @param {string} clientType The type from the verification payload.
   * @returns {boolean} True if the type was successfully updated, false otherwise.
   */
  verify(clientType) {
    if (this.type === "unknown" && (clientType === "ui" || clientType === "rover")) {
      this.type = clientType;
      return true;
    }
    return false;
  }

  /**
   * Sends a V2-compliant message to this specific client.
   * @param {object} message The message object to be sent.
   */
  send(message) {
    if (this.#ws.readyState === this.#ws.OPEN) {
      this.#ws.send(JSON.stringify(message));
    }
  }

  /**
   * Returns a simplified object representation for sending to other UIs.
   */
  toObject() {
    return { id: this.#id, type: this.#type };
  }
}

// --- Express App Setup ---
const app = express();
const frontendPath = path.join(__dirname, "../../frontend");
console.log("Frontend path resolved to:", frontendPath);
app.use(
  express.static(frontendPath, {
    index: "index.html",
  })
);
const server = http.createServer(app);

app.get("/", (req, res) => {
  res.sendFile(path.join(frontendPath, "index.html"));
});

// --- Start the Server ---
server.listen(PORT, HOST, () => {
  console.log(`Server is running on http://localhost:${PORT}`);
  console.log("Serving frontend from:", frontendPath); // This will now work
});
// --- WebSocket Server Setup ---
const wss = new WebSocketServer({ server });

const clients = new Map(); // Stores Client class instances by their unique ID
clientUILinks = new Map();
/**
 * Creates a standard V2 message object.
 * @param {string} type The msgType.
 ** @param {object} payload The msgPayload.
 * @param {string|null} target The msgTarget.
 * @returns {object} A V2 message object.
 */
function createV2Message(type, payload, target = null) {
  return {
    msgID: uuidv4(),
    msgType: type,
    msgSource: SERVER_ID,
    msgTarget: target,
    msgTimestamp: new Date().toISOString(),
    msgPayload: payload,
  };
}

/**
 * Sends a message to all connected clients that are identified as 'ui'.
 * @param {object} message The message object to be sent.
 */
function broadcastToUIs(message) {
  console.log(`Broadcasting to all UIs:`, JSON.stringify(message, null, 2));
  for (const client of clients.values()) {
    if (client.type === "ui") {
      client.send(message);
    }
  }
}
function getCurrentDateTime() {
  return new Date().toISOString();
}
function logTimestamp(s) {
  console.log(`${s} @: ${getCurrentDateTime()}`);
}

// --- WebSocket Connection Handling ---
wss.on("connection", (ws) => {
  // === Step 1: On Connection ===
  const clientId = uuidv4();
  // 1a. Create the Client object. Its type is 'unknown' by default.
  const client = new Client(ws, clientId);
  // 1b. Add this new, unverified client to the main clients map.
  clients.set(clientId, client);
  console.log(`Client ${clientId} connected as 'unknown'. Awaiting verification.`);

  // 1c. Attach the single, permanent ws.on("message", ...) handler.
  ws.on("message", (message) => {
    // === Step 2: On Any Message ===
    logTimestamp(message);
    let parsedMessage;
    try {
      parsedMessage = JSON.parse(message);
    } catch (e) {
      console.error(`Invalid JSON from ${clientId}. Disconnecting.`);
      ws.close();
      return;
    }

    // 2a. Look up the client from the clients map (this will always find it).
    const senderClient = clients.get(clientId);

    // 2b. Use its type property as the gatekeeper check.
    if (senderClient.type === "unknown") {
      // === Step 2c: If client is unverified ===

      // This block handles the verification attempt.
      if (parsedMessage.msgType === "verification") {
        // ADDED CHECK: Ensure msgPayload exists and is a valid object.
        if (parsedMessage.msgPayload && typeof parsedMessage.msgPayload === "object") {
          const clientType = parsedMessage.msgPayload.clientType;

          // If verification is successful, the method updates the client's type.
          if (senderClient.verify(clientType)) {
            // This is the crucial state transition. The client is now "graduated".
            console.log(`Client ${clientId} successfully verified as '${clientType}'.`);

            // TODO: Send the 'assignedID' message back to the client.
            const assignedIDMessage = createV2Message("assignedID", { assignedID: clientId }, clientId);
            senderClient.send(assignedIDMessage);
            // TODO: Broadcast the 'clientConnected' message to all UIs.
          } else {
            // Verification failed (e.g., invalid or missing clientType).
            console.log(`Verification failed for ${clientId}: invalid clientType. Disconnecting.`);
            ws.close();
          }
        } else {
          // Verification failed because the payload was missing or malformed.
          console.log(`Verification failed for ${clientId}: msgPayload is missing or invalid. Disconnecting.`);
          ws.close();
        }
      } else {
        // Verification failed (first message was not of type 'verification').
        console.log(`First message from ${clientId} was not 'verification'. Disconnecting.`);
        ws.close();
      }
    } else {
      // === Step 2d: If client is already verified ===

      // The code skips the verification block and proceeds directly here.
      console.log(`Received message from verified client ${clientId} (${senderClient.type}).`);

      // TODO: Implement the main Message Routing Logic here.
    }
  });

  ws.on("close", () => {
    console.log(`Client ${clientId} disconnected.`);
    clients.delete(clientId);
    // TODO: Broadcast 'clientDisconnected' message to all UIs.
  });

  ws.on("error", (error) => {
    console.error(`WebSocket error from client ${clientId}:`, error);
  });
});
