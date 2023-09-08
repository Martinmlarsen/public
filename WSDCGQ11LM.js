const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const extend = require('zigbee-herdsman-converters/lib/extend');
const e = exposes.presets;
const ea = exposes.access;
const utils = require('zigbee-herdsman-converters/lib/utils');


function calculateAbsoluteHumidity(temperatureCelsius, relativeHumidity) {
    const T = temperatureCelsius
    const Tk = T + 273
    const Rw = 461.5
    const rh = relativeHumidity

    // https://journals.ametsoc.org/view/journals/apme/57/6/jamc-d-17-0334.1.xml
    const psPositiveCelcius = Math.exp(34.494 - (4924.99 / (T + 237.1))) / Math.pow((T + 105), 1.57)
    const psNegativeCelcius = Math.exp(43.494 - (6545.8 / (T + 278))) / Math.pow((T + 868), 2)

    const psToUse = temperatureCelsius > 0 ? psPositiveCelcius : psNegativeCelcius;

    const pa = psToUse * (rh / 100)
    const AH = pa / (Rw * Tk) * 1000

    return AH
}

let xiaomi_temperature2 = {
    cluster: 'msTemperatureMeasurement',
    options: [exposes.options.precision('temperature'), exposes.options.calibration('temperature')],
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        const temperature = parseFloat(msg.data['measuredValue']) / 100.0;

        try {

            let humidity_calibration = parseFloat(options.humidity_calibration)
            let temperature_calibration = parseFloat(options.temperature_calibration)
            const absolute_humidity_property_name = utils.postfixWithEndpointName('absolute_humidity', msg, model, meta);
            const temperature_without_calibration = parseFloat(msg.data['measuredValue']) / 100.0;


            if (!Number.isFinite(humidity_calibration)) {
                humidity_calibration = 0.0;
            }
            
            if (!Number.isFinite(temperature_calibration)) {
                temperature_calibration = 0.0;
            }

            const humidity = msg.endpoint.getClusterAttributeValue('msRelativeHumidity', 'measuredValue') / 100.0 + humidity_calibration
            const temperature_with_calibration = temperature_without_calibration + temperature_calibration;

            let absolute_humidity = utils.precisionRound(calculateAbsoluteHumidity(temperature_with_calibration, humidity), 2)

            // https://github.com/Koenkk/zigbee2mqtt/issues/798
            // Sometimes the sensor publishes non-realistic vales.
            if (temperature > -65 && temperature < 65) {
                return {
                    temperature: utils.calibrateAndPrecisionRoundOptions(temperature, options, 'temperature'),
                    [absolute_humidity_property_name]: absolute_humidity
                };
            }
        } catch (error) {
            meta.logger.error(`Calculation abs humidity (from temperature). ${error}`)
            return {
                temperature: 0,
                absolute_humidity: 0
            };
        }
    }
}
let e_humidity = () => new exposes.Numeric('absolute_humidity', exposes.access.STATE).withUnit('g/m³').withDescription('Measured absolute humidity')

const definition = {
    zigbeeModel: ['lumi.weather'],
    model: 'WSDCGQ11LM',
    vendor: 'Xiaomi',
    description: 'Aqara temperature, humidity and pressure sensor 2235',
    meta: { battery: { voltageToPercentage: '3V_2850_3000' } },
    fromZigbee: [fz.xiaomi_basic, fz.humidity, fz.pressure, xiaomi_temperature2],
    toZigbee: [],
    exposes: [e.battery(), e.humidity(), e.pressure(), e.battery_voltage(), e_humidity(), e.temperature()],
    configure: async (device, coordinatorEndpoint, logger) => {
        device.powerSource = 'Battery';
        device.save();
    },
}

module.exports = definition;

