(() => {
  // interface/src/index.jsx
  var moduleUiPluginMeta = {
    moduleName: "labjack_t7",
    taskTypes: ["labjack.digital_write", "labjack.stream"]
  };
  var streamDiagnosticSuffixes = [
    "_stream_target_scan_rate",
    "_stream_scans_per_read",
    "_stream_actual_scan_rate",
    "_stream_device_scan_backlog",
    "_stream_ljm_scan_backlog",
    "_stream_read_number"
  ];
  function taskChannel(task, suffix) {
    return `${task.portal}/${task.name}${suffix}`;
  }
  function readTelemetryValue(liveChannels, channelName, fallback = null) {
    const value = liveChannels?.[channelName]?.channel_data?.value;
    const numericValue = Number(value);
    return Number.isFinite(numericValue) ? numericValue : fallback;
  }
  function normalizeMappings(argumentsPayload, convertChannelValuePathToChannelName, isStream) {
    if (!argumentsPayload || !Array.isArray(argumentsPayload.mappings)) {
      return [];
    }
    return argumentsPayload.mappings.filter((item) => item && typeof item === "object").map((item, index) => ({
      id: `mapping-${index}-${item.register || ""}-${item.channel || ""}`,
      channelType: item.channel_type === "digital" ? "digital" : "analog",
      register: Number.isFinite(Number(item.register)) ? String(item.register) : "",
      channel: typeof item.channel === "string" ? convertChannelValuePathToChannelName(item.channel) : "",
      isStream
    }));
  }
  function stateChannelLabel(channel) {
    if (!channel || !channel.includes("/")) {
      return "";
    }
    const separator = channel.indexOf("/");
    return `${channel.slice(0, separator)}/${channel.slice(separator + 1)}_state`;
  }
  function createModuleUiPlugin(host) {
    const React = host.React;
    const { useEffect, useRef, useState } = React;
    const {
      Button,
      Input,
      Label,
      Select,
      SelectContent,
      SelectItem,
      SelectTrigger,
      SelectValue,
      Separator,
      DialogDescription,
      DialogFooter,
      DialogHeader,
      DialogTitle
    } = host.components;
    const {
      ChannelComboBox,
      convertChannelValuePathToChannelName
    } = host.helpers;
    function LabJackTaskMetricsGui({ task, liveChannels }) {
      if (task.task_type !== "labjack.stream") {
        return null;
      }
      const targetScanRate = readTelemetryValue(
        liveChannels,
        taskChannel(task, "_stream_target_scan_rate"),
        Number(task.arguments?.target_scan_rate ?? 100)
      );
      const scansPerRead = readTelemetryValue(
        liveChannels,
        taskChannel(task, "_stream_scans_per_read"),
        Number(task.arguments?.scans_per_read ?? 10)
      );
      const actualScanRate = readTelemetryValue(liveChannels, taskChannel(task, "_stream_actual_scan_rate"));
      const deviceScanBacklog = readTelemetryValue(liveChannels, taskChannel(task, "_stream_device_scan_backlog"), 0);
      const ljmScanBacklog = readTelemetryValue(liveChannels, taskChannel(task, "_stream_ljm_scan_backlog"), 0);
      const scanMetrics = [
        ["Target Scan", targetScanRate === null ? "N/A" : `${targetScanRate.toFixed(2)} Hz`],
        ["Scans/Read", scansPerRead === null ? "N/A" : String(scansPerRead)],
        ["Actual Scan", actualScanRate === null ? "N/A" : `${actualScanRate.toFixed(2)} Hz`]
      ];
      const backlogMetrics = [
        ["Device Backlog", deviceScanBacklog === null ? "N/A" : String(deviceScanBacklog)],
        ["LJM Backlog", ljmScanBacklog === null ? "N/A" : String(ljmScanBacklog)]
      ];
      function renderMetric([label, value]) {
        return /* @__PURE__ */ React.createElement("div", { key: label, className: "space-y-1 rounded-md border bg-background/40 p-3" }, /* @__PURE__ */ React.createElement("div", { className: "text-xs uppercase text-muted-foreground" }, label), /* @__PURE__ */ React.createElement("div", { className: "break-all text-sm" }, value));
      }
      return /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement("div", { className: "grid grid-cols-3 gap-2" }, scanMetrics.map(renderMetric)), /* @__PURE__ */ React.createElement("div", { className: "grid grid-cols-2 gap-2" }, backlogMetrics.map(renderMetric)));
    }
    function LabJackTaskSecondaryGui({ task }) {
      const args = task.arguments || {};
      const mappings = normalizeMappings(args, convertChannelValuePathToChannelName, task.task_type === "labjack.stream");
      const instanceName = args.module_instance_name || "UNBOUND";
      const previewMappings = mappings.slice(0, 3);
      const hiddenMappingCount = Math.max(mappings.length - previewMappings.length, 0);
      const mappingPreview = mappings.length === 0 ? "NO MAPPINGS" : mappings.length === 1 ? "1 MAPPING" : `${mappings.length} MAPPINGS`;
      return /* @__PURE__ */ React.createElement(React.Fragment, null, /* @__PURE__ */ React.createElement(Separator, null), /* @__PURE__ */ React.createElement("div", { className: "grid grid-cols-2 gap-2 text-xs" }, /* @__PURE__ */ React.createElement("div", { className: "min-w-0 rounded-md border bg-muted/40 px-3 py-2" }, /* @__PURE__ */ React.createElement("div", { className: "text-muted-foreground" }, "MODULE"), /* @__PURE__ */ React.createElement("div", { className: "truncate" }, instanceName)), /* @__PURE__ */ React.createElement("div", { className: "min-w-0 rounded-md border bg-muted/40 px-3 py-2" }, /* @__PURE__ */ React.createElement("div", { className: "text-muted-foreground" }, "MAPPINGS"), /* @__PURE__ */ React.createElement("div", { className: "truncate" }, mappingPreview))), /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement("div", { className: "text-[10px] uppercase text-muted-foreground" }, "Mapping Preview"), previewMappings.length > 0 ? /* @__PURE__ */ React.createElement("div", { className: "flex flex-wrap gap-2 text-xs" }, previewMappings.map((mapping) => /* @__PURE__ */ React.createElement("div", { key: mapping.id, className: "truncate rounded-md border bg-muted px-2 py-1" }, mapping.isStream ? mapping.channelType === "digital" ? "DIO" : "AIN" : "DIO", mapping.register, " ", "->", " ", mapping.channel)), hiddenMappingCount > 0 ? /* @__PURE__ */ React.createElement("div", { className: "rounded-md border border-dashed px-2 py-1 text-muted-foreground" }, "+", hiddenMappingCount, " more") : null) : /* @__PURE__ */ React.createElement("div", { className: "text-xs text-muted-foreground" }, "No mappings configured.")));
    }
    function useModuleInstances(operation) {
      const [moduleInstances, setModuleInstances] = useState([]);
      useEffect(() => {
        let active = true;
        async function loadModuleInstances() {
          const result = await operation("dartwic/get-module-instances", {
            registry_name: moduleUiPluginMeta.moduleName
          }, 15e3);
          if (active && !result?.error) {
            setModuleInstances(result?.payload?.module_instances || []);
          }
        }
        void loadModuleInstances();
        return () => {
          active = false;
        };
      }, [operation]);
      return moduleInstances;
    }
    function MappingRow({ mapping, isStream, onChange, onRemove, removeDisabled }) {
      return /* @__PURE__ */ React.createElement(
        "div",
        {
          className: "grid items-center gap-2",
          style: { gridTemplateColumns: isStream ? "130px 100px minmax(0, 1fr) auto" : "100px minmax(0, 1fr) minmax(0, 1fr) auto" }
        },
        isStream ? /* @__PURE__ */ React.createElement(
          Select,
          {
            value: mapping.channelType,
            onValueChange: (value) => onChange({ ...mapping, channelType: value })
          },
          /* @__PURE__ */ React.createElement(SelectTrigger, { className: "w-full" }, /* @__PURE__ */ React.createElement(SelectValue, { placeholder: "TYPE" })),
          /* @__PURE__ */ React.createElement(SelectContent, null, /* @__PURE__ */ React.createElement(SelectItem, { value: "analog" }, "ANALOG"), /* @__PURE__ */ React.createElement(SelectItem, { value: "digital" }, "DIGITAL"))
        ) : null,
        /* @__PURE__ */ React.createElement(
          Input,
          {
            type: "number",
            min: "0",
            placeholder: "REGISTER",
            value: mapping.register,
            onChange: (event) => onChange({ ...mapping, register: event.target.value })
          }
        ),
        /* @__PURE__ */ React.createElement(
          ChannelComboBox,
          {
            mode: isStream ? "write" : "read",
            showFieldSelector: false,
            initialValue: mapping.channel,
            placeholder: "",
            onSelect: (value) => onChange({
              ...mapping,
              channel: convertChannelValuePathToChannelName(value)
            }),
            className: "min-w-0 w-full"
          }
        ),
        !isStream ? /* @__PURE__ */ React.createElement("div", { className: "truncate rounded-md border bg-muted/40 px-3 py-2 text-xs text-muted-foreground" }, stateChannelLabel(mapping.channel) || "STATE CHANNEL") : null,
        /* @__PURE__ */ React.createElement(Button, { variant: "ghost", onClick: onRemove, disabled: removeDisabled }, "REMOVE")
      );
    }
    function LabJackTaskDetailGui({ task, operation, onSaved, onClose }) {
      const mappingIdRef = useRef(0);
      const moduleInstances = useModuleInstances(operation);
      const isStream = task.task_type === "labjack.stream";
      const [selectedInstance, setSelectedInstance] = useState(task.arguments?.module_instance_name || "");
      const [targetScanRate, setTargetScanRate] = useState(String(task.arguments?.target_scan_rate ?? 100));
      const [scansPerRead, setScansPerRead] = useState(String(task.arguments?.scans_per_read ?? 10));
      const [mappings, setMappings] = useState(
        () => normalizeMappings(task.arguments, convertChannelValuePathToChannelName, isStream).map((mapping) => ({
          ...mapping,
          id: `mapping-${mappingIdRef.current++}`
        }))
      );
      const [errorMessage, setErrorMessage] = useState("");
      const [isSaving, setIsSaving] = useState(false);
      useEffect(() => {
        setSelectedInstance(task.arguments?.module_instance_name || "");
        setTargetScanRate(String(task.arguments?.target_scan_rate ?? 100));
        setScansPerRead(String(task.arguments?.scans_per_read ?? 10));
        setMappings(
          normalizeMappings(task.arguments, convertChannelValuePathToChannelName, isStream).map((mapping) => ({
            ...mapping,
            id: `mapping-${mappingIdRef.current++}`
          }))
        );
        setErrorMessage("");
      }, [task, isStream]);
      async function saveTask() {
        const cleanedMappings = mappings.map((mapping) => ({
          ...isStream ? { channel_type: mapping.channelType === "digital" ? "digital" : "analog" } : {},
          register: Number(mapping.register),
          channel: mapping.channel.trim()
        })).filter((mapping) => Number.isFinite(mapping.register) && mapping.channel !== "");
        if (!selectedInstance) {
          setErrorMessage("SELECT A LABJACK MODULE INSTANCE.");
          return;
        }
        if (cleanedMappings.length === 0) {
          setErrorMessage("ADD AT LEAST ONE MAPPING.");
          return;
        }
        setIsSaving(true);
        setErrorMessage("");
        try {
          const argumentsPayload = {
            module_instance_name: selectedInstance,
            mappings: cleanedMappings
          };
          if (isStream) {
            argumentsPayload.target_scan_rate = Number(targetScanRate);
            argumentsPayload.scans_per_read = Number(scansPerRead);
          }
          const result = await operation("dartwic/create-task", {
            portal_name: task.portal,
            task_name: task.name,
            task_type: task.task_type,
            arguments: argumentsPayload
          }, 3e4);
          if (result?.error) {
            setErrorMessage((result?.payload?.error || "FAILED TO SAVE TASK.").toUpperCase());
            return;
          }
          if (onSaved) {
            await onSaved();
          }
          onClose?.();
        } finally {
          setIsSaving(false);
        }
      }
      return /* @__PURE__ */ React.createElement(React.Fragment, null, /* @__PURE__ */ React.createElement(DialogHeader, null, /* @__PURE__ */ React.createElement(DialogTitle, null, isStream ? "LABJACK STREAM TASK" : "LABJACK DIGITAL WRITE TASK"), /* @__PURE__ */ React.createElement(DialogDescription, null, isStream ? "MAP LABJACK STREAM REGISTERS TO RAPID CHANNELS." : "MAP RAPID CHANNELS TO LABJACK DIGITAL OUTPUTS WITH READBACK STATE.")), /* @__PURE__ */ React.createElement("div", { className: "space-y-4 py-4" }, /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement(Label, null, "MODULE INSTANCE"), /* @__PURE__ */ React.createElement(Select, { value: selectedInstance, onValueChange: setSelectedInstance }, /* @__PURE__ */ React.createElement(SelectTrigger, { className: "w-full" }, /* @__PURE__ */ React.createElement(SelectValue, { placeholder: "SELECT A LABJACK MODULE INSTANCE" })), /* @__PURE__ */ React.createElement(SelectContent, null, moduleInstances.map((instance) => /* @__PURE__ */ React.createElement(SelectItem, { key: instance.name, value: instance.name }, instance.name))))), isStream ? /* @__PURE__ */ React.createElement("div", { className: "grid grid-cols-2 gap-3" }, /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement(Label, null, "TARGET SCAN RATE"), /* @__PURE__ */ React.createElement(Input, { type: "number", min: "1", value: targetScanRate, onChange: (event) => setTargetScanRate(event.target.value) })), /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement(Label, null, "SCANS PER READ"), /* @__PURE__ */ React.createElement(Input, { type: "number", min: "1", value: scansPerRead, onChange: (event) => setScansPerRead(event.target.value) }))) : null, /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement("div", { className: "flex items-center justify-between gap-2" }, /* @__PURE__ */ React.createElement(Label, null, "MAPPINGS"), /* @__PURE__ */ React.createElement(
        Button,
        {
          variant: "outline",
          onClick: () => setMappings((current) => current.concat([{
            id: `mapping-${mappingIdRef.current++}`,
            channelType: "analog",
            register: "",
            channel: "",
            isStream
          }]))
        },
        "ADD"
      )), /* @__PURE__ */ React.createElement("div", { className: "max-h-72 space-y-2 overflow-y-auto" }, mappings.length === 0 ? /* @__PURE__ */ React.createElement("div", { className: "rounded-md border border-dashed px-3 py-4 text-sm text-muted-foreground" }, "NO MAPPINGS CONFIGURED.") : mappings.map((mapping, index) => /* @__PURE__ */ React.createElement(
        MappingRow,
        {
          key: mapping.id,
          mapping,
          isStream,
          onChange: (nextMapping) => setMappings(
            (current) => current.map(
              (item, itemIndex) => itemIndex === index ? nextMapping : item
            )
          ),
          onRemove: () => setMappings(
            (current) => current.filter((_, itemIndex) => itemIndex !== index)
          ),
          removeDisabled: isSaving
        }
      )))), errorMessage ? /* @__PURE__ */ React.createElement("div", { className: "rounded-md border border-red-500/40 bg-red-500/10 px-3 py-2 text-sm text-red-200" }, errorMessage) : null), /* @__PURE__ */ React.createElement(DialogFooter, null, /* @__PURE__ */ React.createElement(Button, { variant: "ghost", onClick: onClose, disabled: isSaving }, "CANCEL"), /* @__PURE__ */ React.createElement(Button, { onClick: saveTask, disabled: isSaving }, isSaving ? "SAVING" : "SAVE")));
    }
    function LabJackModuleConfigPage({ instanceConfig, setInstanceConfig, save, moduleConfig = {} }) {
      const [isSaving, setIsSaving] = useState(false);
      const parameters = instanceConfig?.parameters || {};
      function updateParameterField(key, value) {
        setInstanceConfig((prev) => ({
          ...prev,
          parameters: {
            ...prev?.parameters || {},
            [key]: value
          }
        }));
      }
      async function handleSave() {
        setIsSaving(true);
        try {
          await save();
        } finally {
          setIsSaving(false);
        }
      }
      return /* @__PURE__ */ React.createElement("div", { className: "flex flex-col gap-4" }, /* @__PURE__ */ React.createElement("div", { className: "flex h-fit w-fit flex-row items-center gap-5 rounded-lg border border-border p-2" }, moduleConfig.icon || moduleConfig.icon_image_src ? /* @__PURE__ */ React.createElement("img", { className: "h-[30px]", src: moduleConfig.icon || moduleConfig.icon_image_src }) : null, /* @__PURE__ */ React.createElement(Label, { className: "text-lg" }, moduleConfig.title || "LabJack T7")), /* @__PURE__ */ React.createElement("div", { className: "flex h-fit w-fit flex-row justify-center gap-4" }, /* @__PURE__ */ React.createElement("div", { className: "flex flex-row items-center gap-5 rounded-lg border border-border p-2" }, /* @__PURE__ */ React.createElement(Label, null, instanceConfig?.name || "")), /* @__PURE__ */ React.createElement(Button, { variant: "outline", disabled: isSaving, onClick: handleSave }, isSaving ? "SAVING" : "SAVE CONFIG")), /* @__PURE__ */ React.createElement("div", { className: "flex flex-col gap-3 rounded-lg border border-border p-4" }, /* @__PURE__ */ React.createElement(Label, { className: "text-md font-semibold" }, "Connection"), [
        ["device_type", "Device Type", "T7"],
        ["connection_type", "Connection Type", "ANY"],
        ["identifier", "Identifier", "-2 or LJM_DEMO_MODE"]
      ].map(([key, label, placeholder]) => /* @__PURE__ */ React.createElement("div", { key, className: "flex flex-col gap-1" }, /* @__PURE__ */ React.createElement(Label, null, label), /* @__PURE__ */ React.createElement(
        Input,
        {
          value: parameters[key] ?? "",
          placeholder,
          onChange: (event) => updateParameterField(key, event.target.value)
        }
      )))));
    }
    return {
      id: moduleUiPluginMeta.moduleName,
      moduleName: moduleUiPluginMeta.moduleName,
      taskTypes: moduleUiPluginMeta.taskTypes,
      ModuleConfigPage: LabJackModuleConfigPage,
      TaskMetricsGui: LabJackTaskMetricsGui,
      shouldUseTaskMetricsGui: (task) => task.task_type === "labjack.stream",
      TaskSecondaryGui: LabJackTaskSecondaryGui,
      TaskDetailGui: LabJackTaskDetailGui,
      getTaskTelemetryChannels: (task) => task.task_type === "labjack.stream" ? streamDiagnosticSuffixes.map((suffix) => taskChannel(task, suffix)) : []
    };
  }

  // interface/src/runtime-entry.jsx
  (function registerModuleUiPlugin() {
    const globalRegistry = window.__dartwicPluginRegistry__ = window.__dartwicPluginRegistry__ || {};
    globalRegistry[moduleUiPluginMeta.moduleName] = {
      createPlugin: createModuleUiPlugin
    };
  })();
})();
