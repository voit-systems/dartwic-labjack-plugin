export const moduleUiPluginMeta = {
    moduleName: "labjack_t7",
    taskTypes: ["labjack.digital_write", "labjack.stream"]
};

const streamDiagnosticSuffixes = [
    "_stream_target_scan_rate",
    "_stream_scans_per_read",
    "_stream_actual_scan_rate",
    "_stream_device_scan_backlog",
    "_stream_ljm_scan_backlog",
    "_stream_last_read_ms"
];

function taskChannel(task, suffix) {
    return `${task.portal}/${task.name}${suffix}`;
}

function readTelemetryValue(liveChannels, channelName, fallback = null) {
    const value = liveChannels?.[channelName]?.channel_data?.value;
    const numericValue = Number(value);
    return Number.isFinite(numericValue) ? numericValue : fallback;
}

function moduleDeviceConnectedChannel(task) {
    const instanceName = task.arguments?.module_instance_name;
    return instanceName ? `${instanceName}/device_connected` : null;
}

function normalizeMappings(argumentsPayload, convertChannelValuePathToChannelName, isStream) {
    if (!argumentsPayload || !Array.isArray(argumentsPayload.mappings)) {
        return [];
    }

    return argumentsPayload.mappings
        .filter((item) => item && typeof item === "object")
        .map((item, index) => ({
            id: `mapping-${index}-${item.register || ""}-${item.channel || ""}`,
            channelType: item.channel_type === "digital" ? "digital" : "analog",
            register: Number.isFinite(Number(item.register)) ? String(item.register) : "",
            channel: typeof item.channel === "string"
                ? convertChannelValuePathToChannelName(item.channel)
                : "",
            isStream
        }));
}

export function createModuleUiPlugin(host) {
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

        const scanMetrics = [
            ["Target Scan", targetScanRate === null ? "N/A" : `${targetScanRate.toFixed(2)} Hz`],
            ["Scans/Read", scansPerRead === null ? "N/A" : String(scansPerRead)],
            ["Actual Scan", actualScanRate === null ? "N/A" : `${actualScanRate.toFixed(2)} Hz`]
        ];

        function renderMetric([label, value]) {
            return (
                <div key={label} className="space-y-1 rounded-md border bg-background/40 p-3">
                    <div className="text-xs uppercase text-muted-foreground">{label}</div>
                    <div className="break-all text-sm">{value}</div>
                </div>
            );
        }

        return (
            <div className="space-y-2">
                <div className="grid grid-cols-3 gap-2">
                    {scanMetrics.map(renderMetric)}
                </div>
            </div>
        );
    }

    function LabJackTaskSecondaryGui({ task, liveChannels }) {
        const args = task.arguments || {};
        const isStream = task.task_type === "labjack.stream";
        const mappings = normalizeMappings(args, convertChannelValuePathToChannelName, task.task_type === "labjack.stream");
        const instanceName = args.module_instance_name || "UNBOUND";
        const connectedValue = readTelemetryValue(liveChannels, moduleDeviceConnectedChannel(task), 0);
        const isConnected = connectedValue !== null && connectedValue !== 0;
        const previewMappings = mappings.slice(0, 3);
        const hiddenMappingCount = Math.max(mappings.length - previewMappings.length, 0);
        const mappingPreview = mappings.length === 0
            ? "NO MAPPINGS"
            : mappings.length === 1
                ? "1 MAPPING"
                : `${mappings.length} MAPPINGS`;
        const lastReadMs = readTelemetryValue(liveChannels, taskChannel(task, "_stream_last_read_ms"), 0);
        const deviceScanBacklog = readTelemetryValue(liveChannels, taskChannel(task, "_stream_device_scan_backlog"), 0);
        const ljmScanBacklog = readTelemetryValue(liveChannels, taskChannel(task, "_stream_ljm_scan_backlog"), 0);
        const streamHealthMetrics = [
            ["LAST READ", lastReadMs === null ? "N/A" : `${lastReadMs.toFixed(2)} ms`],
            ["DEVICE BACKLOG", deviceScanBacklog === null ? "N/A" : String(deviceScanBacklog)],
            ["LJM BACKLOG", ljmScanBacklog === null ? "N/A" : String(ljmScanBacklog)]
        ];

        return (
            <>
                <Separator />
                <div className="grid grid-cols-2 gap-2 text-xs">
                    <div className="min-w-0 rounded-md border bg-muted/40 px-3 py-2">
                        <div className="text-muted-foreground">MODULE</div>
                        <div className="flex min-w-0 items-center gap-2">
                            <div className="min-w-0 truncate">{instanceName}</div>
                            <div
                                className={`shrink-0 rounded px-1.5 py-0.5 text-[9px] font-medium ${
                                    isConnected
                                        ? "border border-green-500/40 bg-green-500/15 text-green-500"
                                        : "border border-red-500/40 bg-red-500/15 text-red-500"
                                }`}
                            >
                                {isConnected ? "CONNECTED" : "DISCONNECTED"}
                            </div>
                        </div>
                    </div>
                    <div className="min-w-0 rounded-md border bg-muted/40 px-3 py-2">
                        <div className="text-muted-foreground">MAPPINGS</div>
                        <div className="truncate">{mappingPreview}</div>
                    </div>
                </div>
                {isStream ? (
                    <div className="grid grid-cols-3 gap-2 text-xs">
                        {streamHealthMetrics.map(([label, value]) => (
                            <div key={label} className="min-w-0 rounded-md border bg-background/40 px-3 py-2">
                                <div className="text-muted-foreground">{label}</div>
                                <div className="truncate">{value}</div>
                            </div>
                        ))}
                    </div>
                ) : null}
                <div className="space-y-2">
                    <div className="text-[10px] uppercase text-muted-foreground">Mapping Preview</div>
                    {previewMappings.length > 0 ? (
                        <div className="flex flex-wrap gap-2 text-xs">
                            {previewMappings.map((mapping) => (
                                <div key={mapping.id} className="truncate rounded-md border bg-muted px-2 py-1">
                                    {(mapping.isStream ? (mapping.channelType === "digital" ? "DIO" : "AIN") : "DIO")}{mapping.register} {"->"} {mapping.channel}
                                </div>
                            ))}
                            {hiddenMappingCount > 0 ? (
                                <div className="rounded-md border border-dashed px-2 py-1 text-muted-foreground">
                                    +{hiddenMappingCount} more
                                </div>
                            ) : null}
                        </div>
                    ) : (
                        <div className="text-xs text-muted-foreground">No mappings configured.</div>
                    )}
                </div>
            </>
        );
    }

    function useModuleInstances(operation) {
        const [moduleInstances, setModuleInstances] = useState([]);

        useEffect(() => {
            let active = true;
            async function loadModuleInstances() {
                const result = await operation("dartwic/get-module-instances", {
                    registry_name: moduleUiPluginMeta.moduleName
                }, 15000);
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
        return (
            <div
                className="grid items-center gap-2"
                style={{ gridTemplateColumns: isStream ? "130px 100px minmax(0, 1fr) auto" : "100px minmax(0, 1fr) auto" }}
            >
                {isStream ? (
                    <Select
                        value={mapping.channelType}
                        onValueChange={(value) => onChange({ ...mapping, channelType: value })}
                    >
                        <SelectTrigger className="w-full">
                            <SelectValue placeholder="TYPE" />
                        </SelectTrigger>
                        <SelectContent>
                            <SelectItem value="analog">ANALOG</SelectItem>
                            <SelectItem value="digital">DIGITAL</SelectItem>
                        </SelectContent>
                    </Select>
                ) : null}
                <Input
                    type="number"
                    min="0"
                    placeholder="REGISTER"
                    value={mapping.register}
                    onChange={(event) => onChange({ ...mapping, register: event.target.value })}
                />
                <ChannelComboBox
                    mode={isStream ? "write" : "read"}
                    showFieldSelector={false}
                    initialValue={mapping.channel}
                    placeholder=""
                    onSelect={(value) =>
                        onChange({
                            ...mapping,
                            channel: convertChannelValuePathToChannelName(value)
                        })
                    }
                    className="min-w-0 w-full"
                />
                <Button variant="ghost" onClick={onRemove} disabled={removeDisabled}>
                    REMOVE
                </Button>
            </div>
        );
    }

    function LabJackTaskDetailGui({ task, operation, onSaved, onClose }) {
        const mappingIdRef = useRef(0);
        const moduleInstances = useModuleInstances(operation);
        const isStream = task.task_type === "labjack.stream";
        const [selectedInstance, setSelectedInstance] = useState(task.arguments?.module_instance_name || "");
        const [targetScanRate, setTargetScanRate] = useState(String(task.arguments?.target_scan_rate ?? 100));
        const [scansPerRead, setScansPerRead] = useState(String(task.arguments?.scans_per_read ?? 10));
        const [mappings, setMappings] = useState(() =>
            normalizeMappings(task.arguments, convertChannelValuePathToChannelName, isStream).map((mapping) => ({
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
            const cleanedMappings = mappings
                .map((mapping) => ({
                    ...(isStream ? { channel_type: mapping.channelType === "digital" ? "digital" : "analog" } : {}),
                    register: Number(mapping.register),
                    channel: mapping.channel.trim()
                }))
                .filter((mapping) => Number.isFinite(mapping.register) && mapping.channel !== "");

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
                }, 30000);

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

        return (
            <>
                <DialogHeader>
                    <DialogTitle>{isStream ? "LABJACK STREAM TASK" : "LABJACK DIGITAL WRITE TASK"}</DialogTitle>
                    <DialogDescription>
                        {isStream ? "MAP LABJACK STREAM REGISTERS TO RAPID CHANNELS." : "MAP RAPID CHANNELS TO LABJACK DIGITAL OUTPUTS WITH READBACK STATE."}
                    </DialogDescription>
                </DialogHeader>
                <div className="space-y-4 py-4">
                    <div className="space-y-2">
                        <Label>MODULE INSTANCE</Label>
                        <Select value={selectedInstance} onValueChange={setSelectedInstance}>
                            <SelectTrigger className="w-full">
                                <SelectValue placeholder="SELECT A LABJACK MODULE INSTANCE" />
                            </SelectTrigger>
                            <SelectContent>
                                {moduleInstances.map((instance) => (
                                    <SelectItem key={instance.name} value={instance.name}>
                                        {instance.name}
                                    </SelectItem>
                                ))}
                            </SelectContent>
                        </Select>
                    </div>
                    {isStream ? (
                        <div className="grid grid-cols-2 gap-3">
                            <div className="space-y-2">
                                <Label>TARGET SCAN RATE</Label>
                                <Input type="number" min="1" value={targetScanRate} onChange={(event) => setTargetScanRate(event.target.value)} />
                            </div>
                            <div className="space-y-2">
                                <Label>SCANS PER READ</Label>
                                <Input type="number" min="1" value={scansPerRead} onChange={(event) => setScansPerRead(event.target.value)} />
                            </div>
                        </div>
                    ) : null}
                    <div className="space-y-2">
                        <div className="flex items-center justify-between gap-2">
                            <Label>MAPPINGS</Label>
                            <Button
                                variant="outline"
                                onClick={() =>
                                    setMappings((current) => current.concat([{
                                        id: `mapping-${mappingIdRef.current++}`,
                                        channelType: "analog",
                                        register: "",
                                        channel: "",
                                        isStream
                                    }]))
                                }
                            >
                                ADD
                            </Button>
                        </div>
                        <div className="max-h-72 space-y-2 overflow-y-auto">
                            {mappings.length === 0 ? (
                                <div className="rounded-md border border-dashed px-3 py-4 text-sm text-muted-foreground">
                                    NO MAPPINGS CONFIGURED.
                                </div>
                            ) : (
                                mappings.map((mapping, index) => (
                                    <MappingRow
                                        key={mapping.id}
                                        mapping={mapping}
                                        isStream={isStream}
                                        onChange={(nextMapping) =>
                                            setMappings((current) =>
                                                current.map((item, itemIndex) =>
                                                    itemIndex === index ? nextMapping : item
                                                )
                                            )
                                        }
                                        onRemove={() =>
                                            setMappings((current) =>
                                                current.filter((_, itemIndex) => itemIndex !== index)
                                            )
                                        }
                                        removeDisabled={isSaving}
                                    />
                                ))
                            )}
                        </div>
                    </div>
                    {errorMessage ? (
                        <div className="rounded-md border border-red-500/40 bg-red-500/10 px-3 py-2 text-sm text-red-200">
                            {errorMessage}
                        </div>
                    ) : null}
                </div>
                <DialogFooter>
                    <Button variant="ghost" onClick={onClose} disabled={isSaving}>
                        CANCEL
                    </Button>
                    <Button onClick={saveTask} disabled={isSaving}>
                        {isSaving ? "SAVING" : "SAVE"}
                    </Button>
                </DialogFooter>
            </>
        );
    }

    function LabJackModuleConfigPage({ instanceConfig, setInstanceConfig, save, moduleConfig = {} }) {
        const [isSaving, setIsSaving] = useState(false);
        const parameters = instanceConfig?.parameters || {};

        function updateParameterField(key, value) {
            setInstanceConfig((prev) => ({
                ...prev,
                parameters: {
                    ...(prev?.parameters || {}),
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

        return (
            <div className="flex flex-col gap-4">
                <div className="flex h-fit w-fit flex-row items-center gap-5 rounded-lg border border-border p-2">
                    {(moduleConfig.icon || moduleConfig.icon_image_src) ? (
                        <img className="h-[30px]" src={moduleConfig.icon || moduleConfig.icon_image_src} />
                    ) : null}
                    <Label className="text-lg">{moduleConfig.title || "LabJack T7"}</Label>
                </div>
                <div className="flex h-fit w-fit flex-row justify-center gap-4">
                    <div className="flex flex-row items-center gap-5 rounded-lg border border-border p-2">
                        <Label>{instanceConfig?.name || ""}</Label>
                    </div>
                    <Button variant="outline" disabled={isSaving} onClick={handleSave}>
                        {isSaving ? "SAVING" : "SAVE CONFIG"}
                    </Button>
                </div>
                <div className="flex flex-col gap-3 rounded-lg border border-border p-4">
                    <Label className="text-md font-semibold">Connection</Label>
                    {[
                        ["device_type", "Device Type", "T7"],
                        ["connection_type", "Connection Type", "ANY"],
                        ["identifier", "Identifier", "-2 or LJM_DEMO_MODE"]
                    ].map(([key, label, placeholder]) => (
                        <div key={key} className="flex flex-col gap-1">
                            <Label>{label}</Label>
                            <Input
                                value={parameters[key] ?? ""}
                                placeholder={placeholder}
                                onChange={(event) => updateParameterField(key, event.target.value)}
                            />
                        </div>
                    ))}
                </div>
            </div>
        );
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
        getTaskTelemetryChannels: (task) => {
            const deviceConnectedChannel = moduleDeviceConnectedChannel(task);
            const channels = deviceConnectedChannel ? [deviceConnectedChannel] : [];
            if (task.task_type === "labjack.stream") {
                channels.push(...streamDiagnosticSuffixes.map((suffix) => taskChannel(task, suffix)));
            }
            return channels;
        }
    };
}
