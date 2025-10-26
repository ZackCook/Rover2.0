document.addEventListener("DOMContentLoaded", () => {
  const ws = new WebSocket(`ws://${window.location.host}`);

  let assignedID = null;

  //DOM components
  const forwardButton = document.getElementById("forwardButton");
  const stopButton = document.getElementById("stopButton");
  const leftButton = document.getElementById("leftButton");
  const rightButton = document.getElementById("rightButton");
  const reverseButton = document.getElementById("reverseButton");
  const cruiseButton = document.getElementById("cruiseButton");

  // ******************************** /
  // ** DOM COMPONENT EVENT HANDLERS ** /
  // ******************************** /
  forwardButton.onmousedown = function () {
    logTimestamp("fwd pressed");
  };

  forwardButton.onmouseup = function () {
    logTimestamp("fwd released");
  };

  // ******************************** /
  // ** WEBSOCKETS EVENT HANDLERS ** /
  // ******************************** /

  ws.addEventListener("open", (event) => {
    logTimestamp(`Connected to server`);

    //send verification data
    const verifyMessage = createV2Message("verification", { clientType: "ui" }, "server-main");
    ws.send(JSON.stringify(verifyMessage));
    logTimestamp("Sent verification message to server");
  });

  ws.addEventListener("close", (event) => {
    logTimestamp("Disconnected from server");
  });

  ws.addEventListener("message", (event) => {
    logTimestamp(`Message from server: ${event.data}`);

    let incomingMessage;
    try {
      incomingMessage = JSON.parse(event.data);
    } catch (e) {
      logTimestamp("incoming message JSON could not be parsed");
    }

    if (incomingMessage.msgType === "assignedID") {
      assignedID = incomingMessage.msgPayload.assignedID;
      logTimestamp(`ID assigned: ${assignedID}`);
    }
  });

  // *********************** /
  // ** SUPPORT FUNCTIONS ** /
  // *********************** /
  function getCurrentDateTime() {
    return new Date().toISOString();
  }

  function logTimestamp(s) {
    console.log(`${s} @: ${getCurrentDateTime()}`);
  }

  function createV2Message(type, payload, target = null) {
    return {
      msgID: crypto.randomUUID(),
      msgType: type,
      msgSource: assignedID,
      msgTarget: target,
      msgTimestamp: new Date().toISOString(),
      msgPayload: payload,
    };
  }
});
